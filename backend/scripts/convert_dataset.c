#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <zlib.h>

#define D            14
#define GZ_CHUNK     (1 << 20)
#define IO_BUF_SZ    (1 << 18)
#define HEADER_PAD   24

typedef struct NumberState {
    int sign;
    int exp_sign;
    int in_frac;
    int in_exp;
    int seen_digit;
    uint64_t int_part;
    uint64_t frac_part;
    uint64_t frac_div;
    int exp_val;
} NumberState;

static void number_reset(NumberState *st) {
    st->sign = 1;
    st->exp_sign = 1;
    st->in_frac = 0;
    st->in_exp = 0;
    st->seen_digit = 0;
    st->int_part = 0;
    st->frac_part = 0;
    st->frac_div = 1;
    st->exp_val = 0;
}

static float fast_pow10(int exp) {
    static const float p10[] = {
        1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f,
        100000.0f, 1000000.0f, 10000000.0f, 100000000.0f, 1000000000.0f,
        10000000000.0f, 100000000000.0f, 1000000000000.0f, 10000000000000.0f,
        100000000000000.0f, 1000000000000000.0f, 10000000000000000.0f,
        100000000000000000.0f, 1000000000000000000.0f
    };
    if (exp == 0) return 1.0f;
    if (exp > 0) {
        if (exp < (int)(sizeof(p10) / sizeof(p10[0]))) return p10[exp];
        float v = p10[18];
        for (int i = 18; i < exp; i++) v *= 10.0f;
        return v;
    }
    exp = -exp;
    if (exp < (int)(sizeof(p10) / sizeof(p10[0]))) return 1.0f / p10[exp];
    float v = 1.0f / p10[18];
    for (int i = 18; i < exp; i++) v /= 10.0f;
    return v;
}

static float number_finalize(const NumberState *st) {
    double v = (double)st->int_part;
    if (st->frac_part && st->frac_div) {
        v += (double)st->frac_part / (double)st->frac_div;
    }
    if (st->exp_val) {
        int e = st->exp_sign * st->exp_val;
        v *= (double)fast_pow10(e);
    }
    if (st->sign < 0) v = -v;
    return (float)v;
}

static int number_feed(NumberState *st, char c, float *out, int *completed) {
    *completed = 0;

    if (c >= '0' && c <= '9') {
        st->seen_digit = 1;
        if (st->in_exp) {
            st->exp_val = (st->exp_val * 10) + (c - '0');
        } else if (st->in_frac) {
            st->frac_part = (st->frac_part * 10) + (c - '0');
            st->frac_div *= 10;
        } else {
            st->int_part = (st->int_part * 10) + (c - '0');
        }
        return 1;
    }

    if (c == '-' && !st->seen_digit && !st->in_exp && !st->in_frac) {
        st->sign = -1;
        return 1;
    }
    if ((c == 'e' || c == 'E') && st->seen_digit && !st->in_exp) {
        st->in_exp = 1;
        return 1;
    }
    if ((c == '-' || c == '+') && st->in_exp && st->exp_val == 0) {
        st->exp_sign = (c == '-') ? -1 : 1;
        return 1;
    }
    if (c == '.' && st->seen_digit && !st->in_frac && !st->in_exp) {
        st->in_frac = 1;
        return 1;
    }

    if (st->seen_digit) {
        *out = number_finalize(st);
        *completed = 1;
        return 0;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *in_path  = argc > 1 ? argv[1] : "/app/resources/references.json.gz";
    const char *bin_path = argc > 2 ? argv[2] : "/app/resources/reference.bin";

    gzFile gz = gzopen(in_path, "rb");
    if (!gz) { fprintf(stderr, "Cannot open: %s\n", in_path); return 1; }
    gzbuffer(gz, GZ_CHUNK * 2);

    FILE *fbin = fopen(bin_path, "wb");
    if (!fbin) { perror("fopen bin"); gzclose(gz); return 1; }

    static char bbuf[IO_BUF_SZ];
    setvbuf(fbin, bbuf, _IOFBF, IO_BUF_SZ);

    uint64_t written = 0;

    char buf[GZ_CHUNK];
    int nread = 0;

    enum {
        ST_SEEK_OBJ = 0,
        ST_IN_OBJ,
        ST_KEY,
        ST_AFTER_KEY,
        ST_VECTOR,
        ST_LABEL_VALUE
    } state = ST_SEEK_OBJ;

    char key[8];
    int key_len = 0;
    int key_is_vector = 0;
    int key_is_label = 0;

    float vec[D];
    int vec_i = 0;
    int have_vec = 0;
    int have_label = 0;
    uint8_t label = 0;

    NumberState num;
    number_reset(&num);

    int in_string = 0;
    int escape = 0;

    int label_pos = 0;
    const char *fraud = "fraud";
    int label_is_fraud = 1;

    for (;;) {
        nread = gzread(gz, buf, (unsigned)sizeof(buf));
        if (nread < 0) { fprintf(stderr, "gzread error\n"); break; }
        if (nread == 0) break;

        for (int i = 0; i < nread; i++) {
            char c = buf[i];

            if (in_string) {
                if (escape) {
                    escape = 0;
                } else if (c == '\\') {
                    escape = 1;
                } else if (c == '"') {
                    in_string = 0;
                    if (state == ST_KEY) {
                        key[key_len] = '\0';
                        key_is_vector = (strcmp(key, "vector") == 0);
                        key_is_label = (strcmp(key, "label") == 0);
                        state = ST_AFTER_KEY;
                    } else if (state == ST_LABEL_VALUE) {
                        label = (label_is_fraud && label_pos >= 5) ? 1 : 0;
                        have_label = 1;
                        state = ST_IN_OBJ;
                        if (have_vec) {
                            uint64_t pad64 = label;
                            fwrite(vec, sizeof(float), D, fbin);
                            fwrite(&pad64, 1, sizeof(pad64), fbin);
                            written++;

                            if (written % 200000 == 0) {
                                fprintf(stderr, "Converted: %llu\n", (unsigned long long)written);
                                fflush(stderr);
                            }

                            have_vec = 0;
                            have_label = 0;
                        }
                    }
                } else {
                    if (state == ST_KEY) {
                        if (key_len < (int)(sizeof(key) - 1)) {
                            key[key_len++] = c;
                        }
                    } else if (state == ST_LABEL_VALUE) {
                        if (label_pos < 5) {
                            if (c != fraud[label_pos]) label_is_fraud = 0;
                        }
                        label_pos++;
                    }
                }
                continue;
            }

            if (c == '"') {
                in_string = 1;
                escape = 0;
                if (state == ST_IN_OBJ) {
                    state = ST_KEY;
                    key_len = 0;
                } else if (state == ST_AFTER_KEY && key_is_label) {
                    state = ST_LABEL_VALUE;
                    label_pos = 0;
                    label_is_fraud = 1;
                }
                continue;
            }

            if (state == ST_SEEK_OBJ) {
                if (c == '{') {
                    state = ST_IN_OBJ;
                    have_vec = 0;
                    have_label = 0;
                }
                continue;
            }

            if (state == ST_IN_OBJ) {
                if (c == '{') {
                    have_vec = 0;
                    have_label = 0;
                }
                if (c == '}') {
                    state = ST_SEEK_OBJ;
                    have_vec = 0;
                    have_label = 0;
                }
                continue;
            }

            if (state == ST_AFTER_KEY) {
                if (c == ':') continue;
                if (key_is_vector && c == '[') {
                    state = ST_VECTOR;
                    vec_i = 0;
                    number_reset(&num);
                }
                continue;
            }

            if (state == ST_VECTOR) {
                if (c == ']') {
                    if (num.seen_digit && vec_i < D) {
                        vec[vec_i++] = number_finalize(&num);
                        number_reset(&num);
                    }
                    if (vec_i == D) have_vec = 1;
                    state = ST_IN_OBJ;
                    if (have_vec && have_label) {
                        uint64_t pad64 = label;
                        fwrite(vec, sizeof(float), D, fbin);
                        fwrite(&pad64, 1, sizeof(pad64), fbin);
                        written++;

                        if (written % 200000 == 0) {
                            fprintf(stderr, "Converted: %llu\n", (unsigned long long)written);
                            fflush(stderr);
                        }

                        have_vec = 0;
                        have_label = 0;
                    }
                    continue;
                }

                if (isspace((unsigned char)c) || c == ',') {
                    if (num.seen_digit) {
                        if (vec_i < D) {
                            vec[vec_i++] = number_finalize(&num);
                        }
                        number_reset(&num);
                    }
                } else {
                    float v = 0.0f;
                    int done = 0;
                    if (!number_feed(&num, c, &v, &done)) {
                        if (done) {
                            if (vec_i < D) {
                                vec[vec_i++] = v;
                            }
                            number_reset(&num);
                        }
                    }
                }
            }

        }
    }



    fflush(fbin);
    fclose(fbin);
    gzclose(gz);

    fprintf(stderr, "Done: %llu records written.\n", (unsigned long long)written);
    return written > 0 ? 0 : 1;
}
