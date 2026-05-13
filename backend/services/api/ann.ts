import { initAnnPool, sendToAnn } from "./annPool.js";

const K = 5;
const _queryBuf = Buffer.alloc(56);
const _result: { dist: number; label: number }[] = Array.from(
    { length: K }, () => ({ dist: 0, label: 0 })
);

const buildQueryBuffer = (vector: Float32Array): void => {
    for (let i = 0; i < 14; i++) {
        _queryBuf.writeFloatLE(Number.isFinite(vector[i]) ? vector[i] : 0, i * 4);
    }
};

export const initAnn = (): Promise<void> => initAnnPool();

export const getTop5Neighbors = async (
    vector: Float32Array
): Promise<{ dist: number; label: number }[]> => {
    buildQueryBuffer(vector);
    const response = await sendToAnn(_queryBuf);
    for (let i = 0; i < K; i++) {
        _result[i].dist = response.error ? 0 : (response.distances[i] ?? 0);
        _result[i].label = response.error ? 0 : (response.labels?.[i] ?? 0);
    }
    return _result;
};