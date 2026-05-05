## rinha-hono-2026

TypeScript + Hono para rinha-de-backend-2026 — uma API de detecção de fraudes avaliada por latência e precisão.

### O que faz

Recebe um payload de transação de cartão, transforma em um vetor de 14 dimensões, encontra os 5 vizinhos mais próximos em um conjunto de dados de referência rotulado usando distância Euclidiana e retorna:

```json
{ "approved": true, "fraud_score": 0.2 }
```

- `approved = fraud_score < 0.6`
- `fraud_score = fraudes_entre_5_vizinhos / 5`

## Arquitetura

```
nginx (balanceador de carga) — escuta em :9999
  ├── api01 — Instância Hono API (porta 3000)
  └── api02 — Instância Hono API (porta 3000)
```

Todos os serviços rodamcom limitações de CPU e memória via Docker Compose.

## Estratégia de Busca

Busca k-NN por força bruta sobre vetores em memória:

- Dataset carregado ao iniciar a partir de `references.json`
- Cada requisição:
  - Vetorização (14 features)
  - Cálculo de distância contra todos os vetores
  - Seleção dos 5 vizinhos mais próximos
- Distância Euclidiana usada para similaridade


## Decisões-chave

| Decisão | Escolha | Motivo |
|---------|---------|--------|
| Framework | Hono | Overhead mínimo, inicialização rápida |
| Linguagem | TypeScript | Produtividade do desenvolvedor + segurança de tipos |
| Método de busca | k-NN por força bruta | Mais simples, determinístico, sem erro de aproximação |
| Métrica de distância | Euclidiana | Direta e suficiente para dados normalizados |
| Carregamento de dados | Streaming (JSON) | Evita picos de memória em datasets grandes |
| Balanceamento de carga | Nginx | Distribuição round-robin simples |

## Layout dos Módulos

```
src/
  index.ts                                 inicialização do servidor (Hono + serve)
  controller/
    api.ts                                 rotas da API (/ready, /fraud-score)
  configs/
    vector-limits.ts                       configuracao de vetorização
  services/
    vetorizedService.ts                    lógica de vetorização (14 features)
    knnService.ts                          distância + vizinhos mais próximos
  dto/
    response.dto.ts                        response DTO
    customer-response.dto.ts               customer DTO
    last_transaction-response.dto.ts       last transaction DTO
    merchant-response.dto.ts               merchant DTO
    terminal-response.dto.ts               terminal DTO
    transaction-response.dto.ts            transaction DTO
  types/
    referenceItem.ts                       tipagem do dataset

resources/
  references.json                          dataset rotulado (~248MB)
  mcc_risk.json                            mapeamento de risco MCC
```

## Desenvolvimento

```bash
docker compose up --build
```

Teste de health check:

```bash
curl http://localhost:9999/ready
```

Teste de detecção de fraude:

```bash
curl -X POST http://localhost:9999/fraud-score \
  -H "Content-Type: application/json" \
  -d '{ ... }'
```

## Notas

- Dataset é carregado em memória uma vez ao iniciar
- Cada requisição executa uma varredura completa (O(N))
- Foco em correção primeiro, performance depois
- Projetado para rodar com limitações de recursos