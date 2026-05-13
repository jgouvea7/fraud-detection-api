import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const jsonPath =
    process.env.MCC_RISK_PATH ??
    (fs.existsSync("/app/resources/mcc_risk.json")
        ? "/app/resources/mcc_risk.json"
        : path.resolve(__dirname, "..", "..", "resources", "mcc_risk.json"));


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