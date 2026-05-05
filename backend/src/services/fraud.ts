

export const analyzeFraud = (top5Neighbors: { dist: number, label: number }[]) => {
    const neighborLabels = top5Neighbors.map(n => n.label);

    const fraudCount = neighborLabels.filter(label => label === 1).length;

    const score = fraudCount / 5;

    const approved = score < 0.6;

    return {
        approved,
        score
    };
};