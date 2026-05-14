import * as fs from "node:fs";
import * as path from "node:path";
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { fileURLToPath } from "node:url";


const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BIN_PATH = process.env.ANN_BIN_PATH ??
    (fs.existsSync("/app/services/native/ann")
        ? "/app/services/native/ann"
        : path.resolve(__dirname, "..", "native", "ann"));

const DATASET_PATH = process.env.ANN_DATASET_PATH ??
    (fs.existsSync("/app/resources/reference.bin")

        ? "/app/resources/reference.bin"

        : path.resolve(__dirname, "..", "..", "resources", "reference.bin"));


const WORKER_COUNT = 1
const MAX_INFLIGHT = 14
const REQUEST_TIMEOUT_MS = 500

export type ANNResponse = {
    indices: number[];
    distances: number[];
    labels?: number[];
    error?: string;
};

class Semaphore {
    private available: number;
    private readonly queue: Array<() => void> = [];

    constructor(limit: number) { this.available = limit; }

    async acquire(): Promise<() => void> {
        if (this.available > 0) {
            this.available--;
            return () => this.release();
        }

        return new Promise((resolve) => {
            this.queue.push(() => {
                this.available--;
                resolve(() => this.release());
            });
        });
    }

    private release(): void {
        this.available++;
        this.queue.shift()?.();
    }
}


class ANNWorker {
    private readonly id: number;
    private child: ChildProcessWithoutNullStreams | null = null;
    private buffer: Buffer = Buffer.alloc(0);
    private ready = false;

    private readyPromise: Promise<void> | null = null;
    private readyResolve: (() => void) | null = null;
    private readyReject: ((e: Error) => void) | null = null;

    private readonly pending: Array<(v: ANNResponse) => void> = [];

    constructor(id: number) { this.id = id; }

    private isAlive(): boolean {
        return (
            this.child != null &&
            !this.child.killed &&
            this.child.exitCode == null &&
            this.child.stdin.writable
        );
    }

    private failAllPending(reason: string): void {
        const snapshot = this.pending.splice(0);
        for (const resolve of snapshot) {
            resolve({ indices: [], distances: [], error: reason });
        }
    }

    private handleDeath(reason: string): void {
        const wasStarting = !this.ready && this.readyResolve !== null;

        this.child = null;
        this.ready = false;
        this.buffer = Buffer.alloc(0);
        this.readyPromise = null;

        if (wasStarting && this.readyReject) {
            this.readyReject(new Error(reason));
        }

        this.readyResolve = null;
        this.readyReject = null;

        this.failAllPending(reason);
        process.stderr.write(`[ann-worker-${this.id}] died: ${reason}\n`);
    }

    private onData = (chunk: Buffer): void => {
        this.buffer = Buffer.concat([this.buffer, chunk]);
        this.processBuffer();
    };

    private processBuffer() {
        if (!this.ready) {
            const idx = this.buffer.indexOf(10);
            if (idx !== -1) {
                this.ready = true;
                this.buffer = this.buffer.subarray(idx + 1);
                const resolve = this.readyResolve;
                this.readyResolve = null;
                this.readyReject = null;
                resolve?.();
            }
        }

        while (this.ready && this.buffer.length >= 60) {
            const chunk = this.buffer.subarray(0, 60);
            this.buffer = this.buffer.subarray(60);

            const resolver = this.pending.shift();
            if (!resolver) continue;

            const indices = new Array(5);
            const distances = new Array(5);
            const labels = new Array(5);

            for (let i = 0; i < 5; i++) {
                indices[i] = chunk.readInt32LE(i * 4);
                distances[i] = chunk.readFloatLE(20 + i * 4);
                labels[i] = chunk.readUInt32LE(40 + i * 4);
            }

            resolver({ indices, distances, labels });
        }
    }

    private spawn(): void {
        const args = ["--dataset", DATASET_PATH];

        const proc = spawn(BIN_PATH, args, { stdio: ["pipe", "pipe", "pipe"] });
        this.child = proc;

        proc.stdout.on("data", this.onData);
        proc.stderr.on("data", (data) => process.stderr.write(`[ann-worker-${this.id} stderr] ${data.toString()}`));
        proc.on("error", (e) => this.handleDeath(`error:${e.message}`));
        proc.on("exit", (code) => this.handleDeath(`exit:${code ?? "signal"}`));
    }

    async ensureReady(): Promise<void> {
        if (this.ready) return;

        if (this.readyPromise) {
            await this.readyPromise;
            return;
        }

        this.readyPromise = new Promise<void>((resolve, reject) => {
            this.readyResolve = resolve;
            this.readyReject = reject;
            this.spawn();
        });

        try {
            await this.readyPromise;
        } catch {
            this.readyPromise = null;
            await new Promise((r) => setTimeout(r, 500));
            await this.ensureReady();
        }
    }

    async send(data: Buffer): Promise<ANNResponse> {
        await this.ensureReady();

        if (!this.isAlive()) {
            this.ready = false;
            this.readyPromise = null;
            await this.ensureReady();
        }

        if (!this.isAlive()) {
            return { indices: [], distances: [], error: "process-not-available" };
        }

        return new Promise<ANNResponse>((resolve) => {
            let settled = false;

            const timer = setTimeout(() => {
                if (settled) return;
                settled = true;
                process.stderr.write(`[ann-worker-${this.id}] request timeout — killing\n`);
                this.child?.kill("SIGKILL");
                resolve({ indices: [], distances: [], error: "timeout" });
            }, REQUEST_TIMEOUT_MS);

            this.pending.push((response) => {
                if (settled) return;
                settled = true;
                clearTimeout(timer);
                resolve(response);
            });

            this.child!.stdin.write(data, (err) => {
                if (err) this.handleDeath(`write-error:${err.message}`);
            });

        });
    }
}


const semaphore = new Semaphore(MAX_INFLIGHT);
const workers = Array.from({ length: WORKER_COUNT }, (_, i) => new ANNWorker(i));
let rrIndex = 0;
let poolReady = false;

const nextWorker = (): ANNWorker => {
    const w = workers[rrIndex % workers.length];
    rrIndex = (rrIndex + 1) % workers.length;
    return w;

};


export const initAnnPool = async (): Promise<void> => {
    if (poolReady) return;
    await Promise.all(workers.map((w) => w.ensureReady()));
    poolReady = true;
    process.stderr.write(`[ann-pool] ${WORKER_COUNT} workers ready\n`);
};


export const sendToAnn = async (data: Buffer): Promise<ANNResponse> => {
    const release = await semaphore.acquire();
    try {
        return await nextWorker().send(data);

    } finally {
        release();
    }
};

void initAnnPool();