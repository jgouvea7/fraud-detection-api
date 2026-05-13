CC     ?= gcc
CFLAGS ?= -O3 -mavx2 -march=native -flto -ffast-math -fno-plt -pipe -Wall -Wextra

all: ann convert_dataset

ann: ann.c ann.h
	$(CC) $(CFLAGS) -o ann ann.c -lm

convert_dataset: ../../scripts/convert_dataset.c
	$(CC) $(CFLAGS) -o convert_dataset ../../scripts/convert_dataset.c -lz

ann-build: ../../scripts/ann_build.c
	$(CC) $(CFLAGS) -o ann-build ../../scripts/ann-build.c

clean:
	rm -f ann convert_dataset
