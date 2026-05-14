import type { ResponseDTO } from "./dto/response.dto.js";
import { VECTOR_LIMITS } from "./configs/vectorLimits.js";
import { getMccRisk } from "./mccRisk.js";


const limite = (val: number) => Math.max(0, Math.min(1, val));


export const transformToVector = (dto: ResponseDTO): Float32Array => {
    const vector = new Float32Array(16);
    const tx = dto.transaction;
    const customer = dto.customer;
    const merchant = dto.merchant;
    const terminal = dto.terminal;
    const last_transaction = dto?.last_transaction;


    const dateRequest = new Date(tx.requested_at);

    let minutes_since_last_tx = -1;
    let km_from_last_tx = -1;

    if (last_transaction) {
        const dateLast = new Date(last_transaction.timestamp);
        const diffMin = Math.abs(dateRequest.getTime() - dateLast.getTime()) / 60000;
        minutes_since_last_tx = limite(diffMin / 1440);
        km_from_last_tx = limite(last_transaction.km_from_current / 1000);
    }


    const amount = tx.amount / VECTOR_LIMITS.MAX_AMOUNT;
    const installments = tx.installments / VECTOR_LIMITS.MAX_INSTALLMENTS;
    const amount_vs_avg = (tx.amount / customer.avg_amount) / VECTOR_LIMITS.AMOUNT_VS_AVG_RATIO;
    const hour_of_day = dateRequest.getUTCHours() / 23;
    const day_of_week = dateRequest.getUTCDay() / 6;
    const km_from_home = terminal.km_from_home / VECTOR_LIMITS.MAX_KM;
    const tx_count_24h = customer.tx_count_24h / VECTOR_LIMITS.MAX_TX_COUNT_24H;
    const is_online = (terminal.is_online == true) ? 1 : 0;
    const card_present = (terminal.card_present == true) ? 1 : 0;
    const knownMerchants = customer.known_merchants;
    const unknown_merchant = !knownMerchants || !knownMerchants.includes(merchant.id) ? 1 : 0;
    const mcc_risk = getMccRisk(merchant.mcc)
    const merchant_avg_amount = merchant.avg_amount / VECTOR_LIMITS.MAX_MERCHANT_AVG_AMOUNT;


    vector[0] = limite(amount);
    vector[1] = limite(installments);
    vector[2] = limite(amount_vs_avg);
    vector[3] = hour_of_day;
    vector[4] = day_of_week;
    vector[5] = minutes_since_last_tx;
    vector[6] = km_from_last_tx;
    vector[7] = limite(km_from_home);
    vector[8] = limite(tx_count_24h);
    vector[9] = is_online;
    vector[10] = card_present;
    vector[11] = unknown_merchant;
    vector[12] = mcc_risk;
    vector[13] = limite(merchant_avg_amount);

    return vector;
}