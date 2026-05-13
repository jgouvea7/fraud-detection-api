import type { CustomerResponseDTO } from "./customer-response.dto.js";
import type { LastTransactionDTO } from "./last_transaction-response.dto.js";
import type { MerchantResponseDTO } from "./merchant-response.dto.js";
import type { TerminalResponseDTO } from "./terminal-response.dto.js";
import type { TransactionResponseDTO } from "./transaction-response.dto.js";

export class ResponseDTO {
    transaction!: TransactionResponseDTO;
    customer!: CustomerResponseDTO;
    merchant!: MerchantResponseDTO;
    terminal!: TerminalResponseDTO;
    last_transaction!: LastTransactionDTO;
}