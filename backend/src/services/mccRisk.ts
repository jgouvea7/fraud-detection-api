import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const jsonPath = path.resolve(__dirname, "../json/mcc_risk.json");


let mccRisks: Record<string, number> = {};

try {
    const rawData = fs.readFileSync(jsonPath, 'utf-8');
    mccRisks = JSON.parse(rawData);
} catch (error) {
    console.error("Erro ao carregar mcc_risk.json. Verifique o caminho:", jsonPath);
}

function getMccRisk(mcc: string | undefined): number {
    if (!mcc) return 0.5;

    const risk = mccRisks[mcc];
    return risk ?? 0.5;
}

export { getMccRisk };