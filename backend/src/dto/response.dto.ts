import type { CustomerResponseDTO } from "./customer-response.dto";
import type { LastTransactionDTO } from "./last_transaction-response.dto";
import type { MerchantResponseDTO } from "./merchant-response.dto";
import type { TerminalResponseDTO } from "./terminal-response.dto";
import type { TransactionResponseDTO } from "./transaction-response.dto";

export class ResponseDTO {
    transaction!: TransactionResponseDTO;
    customer!: CustomerResponseDTO;
    merchant!: MerchantResponseDTO;
    terminal!: TerminalResponseDTO;
    last_transaction!: LastTransactionDTO;
}