export const analyzeFraud = (neighbors: { dist: number; label: number }[]) => {
    const len = neighbors.length;

    if (len === 0) {
        return { approved: false, fraud_score: 1 };
    }

    let fraudWeight = 0;
    let totalWeight = 0;

    for (let i = 0; i < len; i++) {
        const n = neighbors[i];

        const dist = n.dist < 0.001 ? 0.001 : n.dist;
        const w = 1 / dist;

        totalWeight += w;

        if (n.label === 1) {
            fraudWeight += w;
        }
    }

    const fraud_score = fraudWeight / totalWeight;

    return {
        approved: fraud_score < 0.6,
        fraud_score
    };
};