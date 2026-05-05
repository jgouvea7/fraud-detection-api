import { streamArray } from "stream-json/streamers/stream-array.js";
import { parser } from "stream-json";
import * as fs from "fs";
import * as path from "path";
import * as zlib from "zlib";
import { fileURLToPath } from "url";
import type { ReferenceItem } from "../types/referenceItem";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const jsonPath = path.resolve(__dirname, '../resources/references.json.gz');

const vectors: number[][] = []
const labels: number[] = []

export const loadDataset = () => {
    return new Promise((resolve) => {
        const pipeline = fs.createReadStream(jsonPath)
            .pipe(zlib.createGunzip())
            .pipe(parser() as any)
            .pipe(streamArray() as any);

        pipeline.on("data", (data: { value: ReferenceItem }) => {
            vectors.push(data.value.vector);
            labels.push(data.value.fraud ? 1 : 0);
        });

        pipeline.on("end", () => {
            resolve(true);
        });
    });
};

export const getTop5Neighbors = (target: number[]) => {
    let allDistances: { dist: number, label: number }[] = [];

    for (let i = 0; i < vectors.length; i++) {
        let squareDiff = 0;
        const v = vectors[i];
        for (let j = 0; j < 14; j++) {
            const diff = target[j] - v[j];
            squareDiff += diff * diff;
        }
        allDistances.push({ dist: squareDiff, label: labels[i] });
    }

    return allDistances.sort((a, b) => a.dist - b.dist).slice(0, 5);
};

