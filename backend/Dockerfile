FROM alpine:3.19 AS c-builder

RUN apk add --no-cache gcc musl-dev zlib-dev make

WORKDIR /build/native
COPY services/native/ .
RUN make ann CC=gcc CFLAGS="-O3 -mavx2 -mfma -march=x86-64-v3 -flto -ffast-math -fno-plt -pipe"

WORKDIR /build/scripts
COPY scripts/convert_dataset.c .
COPY scripts/ann_build.c .
RUN gcc -O3 -mavx2 -mfma -march=x86-64-v3 -flto -ffast-math -fno-plt -pipe \
        -o convert_dataset convert_dataset.c -lz
RUN gcc -O3 -mavx2 -mfma -march=x86-64-v3 -flto -ffast-math -fno-plt -pipe \
        -o ann_build ann_build.c

WORKDIR /build/output
COPY resources/references.json.gz .
RUN /build/scripts/convert_dataset \
        references.json.gz \
        reference.bin
RUN /build/scripts/ann_build \
        --dataset /build/output/reference.bin \
        --output  /build/output/tree.bin


FROM node:20-alpine AS ts-builder

WORKDIR /app
COPY package*.json ./
RUN npm ci

COPY . .
RUN npm run build


FROM node:20-alpine AS runtime

WORKDIR /app

COPY --from=ts-builder /app/node_modules ./node_modules
COPY --from=ts-builder /app/dist         ./dist

COPY --from=c-builder /build/native/ann            ./services/native/ann
COPY --from=c-builder /build/output/reference.bin  ./resources/reference.bin
COPY --from=c-builder /build/output/tree.bin        ./resources/tree.bin

COPY resources/mcc_risk.json      ./resources/mcc_risk.json

RUN chmod +x ./services/native/ann

EXPOSE 9999

CMD ["node", "--max-old-space-size=120", "dist/index.js"]