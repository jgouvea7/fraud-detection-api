import type { ResponseDTO } from "../dto/response.dto";
import { VECTOR_LIMITS } from "../configs/vector-limits";
import { getMccRisk } from "./mccRisk";


const limite = (val: number) => Math.max(0, Math.min(1, val));

export const  transformToVector = (dto: ResponseDTO): number[] => {
    const tx = dto.transaction;
    const customer = dto.customer;
    const merchant = dto.merchant;
    const terminal = dto.terminal;
    const last_transaction = dto?.last_transaction;


    let minutes_since_last_tx: number;
    let km_from_last_tx: number;

    const dateRequest = new Date(tx.requested_at);

    if(!last_transaction) {
        minutes_since_last_tx = -1;
        km_from_last_tx = -1;
    } else {
        const dateLastTransaction = new Date(last_transaction.timestamp);
        minutes_since_last_tx = dateLastTransaction.getMinutes() / VECTOR_LIMITS.MAX_MINUTES;
        km_from_last_tx = last_transaction.km_from_current / VECTOR_LIMITS.MAX_KM;
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
    const unknown_merchant = (!dto.merchant.id) ? 1 : 0;
    const mcc_risk = getMccRisk(merchant.mcc)
    const merchant_avg_amount = merchant.avg_amount / VECTOR_LIMITS.MAX_MERCHANT_AVG_AMOUNT;


    return [
        limite(amount),
        limite(installments),
        limite(amount_vs_avg),
        hour_of_day,
        day_of_week,
        limite(minutes_since_last_tx),
        limite(km_from_last_tx),
        limite(km_from_home),
        limite(tx_count_24h),
        is_online,
        card_present,
        unknown_merchant,
        mcc_risk,
        limite(merchant_avg_amount)
    ]
}