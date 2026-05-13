export const analyzeFraud = (top5Neighbors: { dist: number, label: number }[]) => {
    if (!top5Neighbors || top5Neighbors.length === 0) {
        return { approved: false, fraud_score: 1.0 };
    }

    let fraudWeight = 0;
    let totalWeight = 0;

    for (const n of top5Neighbors) {
        const w = 1 / (n.dist + 1e-6);
        totalWeight += w;
        if (n.label === 1) fraudWeight += w;
    }

    const fraud_score = fraudWeight / totalWeight;

    return {
        approved: fraud_score < 0.25,
        fraud_score
    };
};