import { initAnnPool, sendToAnn } from "./annPool.js";

const K = 5;

const buildQueryBuffer = (vector: Float32Array): Buffer => {
    const queryBuf = Buffer.allocUnsafe(56);

    for (let i = 0; i < 14; i++) {
        queryBuf.writeFloatLE(
            Number.isFinite(vector[i]) ? vector[i] : 0,
            i * 4
        );
    }

    return queryBuf;
};

export const initAnn = (): Promise<void> => initAnnPool();

export const getTop5Neighbors = async (
    vector: Float32Array
): Promise<{ dist: number; label: number }[]> => {

    const queryBuf = buildQueryBuffer(vector);

    const response = await sendToAnn(queryBuf);

    const result = new Array(K);

    for (let i = 0; i < K; i++) {
        result[i] = {
            dist: response.error ? 0 : (response.distances[i] ?? 0),
            label: response.error ? 0 : (response.labels?.[i] ?? 0)
        };
    }

    return result;
};