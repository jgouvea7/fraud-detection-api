## rinha-hono-2026

TypeScript + C para rinha-de-backend-2026 — uma API de detecção de fraudes avaliada por latência e precisão.

### O que faz

Recebe um payload de transação, transforma em um vetor de 14 dimensões, realiza uma busca ANN (*Approximate Nearest Neighbors*) para encontrar os 5 vizinhos mais próximos em um conjunto de dados rotulado usando distância Euclidiana e retorna:

```json
{ "approved": true, "fraud_score": 0.2 }
```

## Arquitetura

```
nginx (balanceador de carga) — escuta em :9999
  ├── api01 — Instância Hono API (porta 9999)
  └── api02 — Instância Hono API (porta 9999)
```

## Estratégia de Busca

Busca ANN utilizando KD-Tree otimizada em C com AVX2.

- Dataset e árvore são buildados junto da imagem Docker
- Dataset carregado em memória via `mmap`
- Busca realizada por travessia da KD-Tree
- Distância Euclidiana usada para similaridade
- Retorno dos 5 vizinhos mais próximos
- Fallback para brute-force caso a árvore não esteja disponível


## Layout dos Módulos

```
src/
  resources/
    references.json.gz                          dataset zipado
    mcc_risk.json                               mapeamento de risco MCC
  scripts/
    ann_build.c                                 build e serialização da KD-Tree
    convert_dataset.c                           conversão do dataset JSON para binário              
  services/
    api/
      dto/
        response.dto.ts                         response DTO
        customer-response.dto.ts                customer DTO
        last_transaction-response.dto.ts        last transaction DTO
        merchant-response.dto.ts                merchant DTO
        terminal-response.dto.ts                terminal DTO
        transaction-response.dto.ts             transaction DTO
      types/
        referenceItem.ts                        tipagem do dataset
      configs/
        vectorLimits.ts                         configuração de vetorização
      vectorized.ts                             lógica de vetorização (14 features)
      controller.ts                             rotas da API (/ready, /fraud-score)
      ann.ts                                    integração TypeScript com engine ANN
      annPool.ts                                pool de workers/processos da ANN
      fraud.ts                                  cálculo de score de fraude
      mccRisk.ts                                

    native/
      ann.c                                     engine ANN/KD-Tree otimizada em C
      ann.h                                     interface nativa da busca ANN
 
  index.ts                                      inicialização do servidor (Hono + serve) 

```

## Desenvolvimento

```bash
docker compose up -d --build
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