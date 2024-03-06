#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <immintrin.h>
#include <string.h>
#include <ctype.h>

#include <sys/mman.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "ini.h"

#if 0
#define DEBUG fprintf(stderr, "%d -> %d :: %s\n", __LINE__, ctx->state, ptr)
#else
#define DEBUG
#endif

static void ini_print(__m256i d)
{
    int arr[32] = {
        _mm256_extract_epi8(d,  0),
        _mm256_extract_epi8(d,  1),
        _mm256_extract_epi8(d,  2),
        _mm256_extract_epi8(d,  3),
        _mm256_extract_epi8(d,  4),
        _mm256_extract_epi8(d,  5),
        _mm256_extract_epi8(d,  6),
        _mm256_extract_epi8(d,  7),
        _mm256_extract_epi8(d,  8),
        _mm256_extract_epi8(d,  9),
        _mm256_extract_epi8(d, 10),
        _mm256_extract_epi8(d, 11),
        _mm256_extract_epi8(d, 12),
        _mm256_extract_epi8(d, 13),
        _mm256_extract_epi8(d, 14),
        _mm256_extract_epi8(d, 15),

        _mm256_extract_epi8(d, 16),
        _mm256_extract_epi8(d, 17),
        _mm256_extract_epi8(d, 18),
        _mm256_extract_epi8(d, 19),
        _mm256_extract_epi8(d, 20),
        _mm256_extract_epi8(d, 21),
        _mm256_extract_epi8(d, 22),
        _mm256_extract_epi8(d, 23),
        _mm256_extract_epi8(d, 24),
        _mm256_extract_epi8(d, 25),
        _mm256_extract_epi8(d, 26),
        _mm256_extract_epi8(d, 27),
        _mm256_extract_epi8(d, 28),
        _mm256_extract_epi8(d, 29),
        _mm256_extract_epi8(d, 30),
        _mm256_extract_epi8(d, 31),
    };

    for (int i = 0; i < sizeof(arr) / sizeof(*arr); i++) {
        printf("[%d] = %d\n", i, arr[i]);
    }
}

static __m256i ini_load(const char *ptr, uint64_t len)
{
    __m256i out;
    __m128i a;

    if (len >= 32) {
        out = _mm256_lddqu_si256((__m256i *)ptr);
        return out;
    }

    if (len >= 16) {
        a = _mm_lddqu_si128((__m128i *)ptr);
        out = _mm256_insertf128_si256(out, a, 0);
    }

    a = _mm_set1_epi8(0);

    for (uint64_t i = len % 16; i > 0; i--) {
        a = _mm_slli_si128(a, 1);
        a = _mm_insert_epi8(a, ptr[(len / 16) + (i - 1)], 0);
    }

    if (len >= 16) {
        out = _mm256_insertf128_si256(out, a, 1);
    } else {
        out = _mm256_insertf128_si256(out, a, 0);
        a = _mm_set1_epi8(0);
        out = _mm256_insertf128_si256(out, a, 1);
    }

    return out;
}

static bool ini_do(struct ini_ctx *ctx, const char *ptr, size_t len)
{
    __m256i inv = _mm256_set1_epi8(0xff);
    __m256i space = _mm256_set1_epi8(' ');
    __m256i tab = _mm256_set1_epi8('\t');
    __m256i line_feed = _mm256_set1_epi8('\n');
    __m256i left_square_bracket = _mm256_set1_epi8('[');
    __m256i right_square_bracket = _mm256_set1_epi8(']');
    __m256i hashtag = _mm256_set1_epi8('#');
    __m256i equal = _mm256_set1_epi8('=');

    const char *se, *sb = NULL;
    const char *ke, *kb = NULL;
    const char *ve, *vb = NULL;

    int off = 0;

    for (;;) {
        if (len == 0) {
            printf("len\n");
            return true;
        }

        __m256i d = ini_load(ptr, len);

        __m256i eq0 = _mm256_cmpeq_epi8(d, space);
        eq0 = _mm256_xor_si256(eq0, inv);

        int mask0 = _mm256_movemask_epi8(eq0);

        __m256i eq1 = _mm256_cmpeq_epi8(d, left_square_bracket);
        __m256i eq2 = _mm256_cmpeq_epi8(d, right_square_bracket);

        int mask_lsb = _mm256_movemask_epi8(eq1);
        int mask_rsb = _mm256_movemask_epi8(eq2);
        eq0 = _mm256_or_si256(eq1, eq2);

        __m256i eq3 = _mm256_cmpeq_epi8(d, equal);
        __m256i eq4 = _mm256_cmpeq_epi8(d, line_feed);
        eq1 = _mm256_or_si256(eq3, eq4);

        __m256i eq5 = _mm256_cmpeq_epi8(d, hashtag);
        eq0 = _mm256_or_si256(eq0, eq1);

        eq0 = _mm256_or_si256(eq0, eq5);

        int mask = _mm256_movemask_epi8(eq0);

        // short circuit, skip all spaces
        if (mask0 == 0) {
            ptr += (len < 16) ? len : 16;
            len -= (len < 16) ? len : 16;
            continue;
        }

        int mmask = 0xffffffff;

        // all spaces, skip
        switch (ctx->state) {
        int at, tmp;

        _ini_state_begin_line: ctx->state = ini_state_begin_line;
        case ini_state_begin_line:
            DEBUG;
            at = __builtin_ctz(mask | mask0);

            switch (ptr[at]) {
            case '[':
                mask0 &= ~((1 << (at + 1)) - 1);
                mask  &= ~(1 << at);
                goto _ini_state_begin_section;
            case '\n':
                mask0 &= ~((1 << (at + 1)) - 1);
                mask  &= ~(1 << at);
                goto _ini_state_begin_line;
            default:
                if (!isalpha(ptr[at])) {
                    printf("expected key or section\n");
                    return false;
                }

                kb = &ptr[at];
                goto _ini_state_in_key;
            }

            break;
        _ini_state_begin_section: ctx->state = ini_state_begin_section;
        case ini_state_begin_section:
            DEBUG;
            at = __builtin_ctz(mask | mask0);
            sb = ptr + at;
            goto _ini_state_in_section;
        _ini_state_in_section: ctx->state = ini_state_in_section;
        case ini_state_in_section:
            DEBUG;
            tmp = mask0 & ((mask | (mask - 1)) ^ mask);

            at = (8*sizeof(tmp)) - __builtin_clz(tmp);
            se = &ptr[at];

            // no control char
            if (mask == 0) {
                ptr += (len < 16) ? len : 16;
                len -= (len < 16) ? len : 16;
                continue;
            }

            at = __builtin_ctz(mask);

            if (ptr[at] != ']') {
                printf("invalid\n");
                return false;
            }

            mask0 &= ~((1 << (at + 1)) - 1);
            mask &= ~(1 << at);
            goto _ini_state_begin_line;
        _ini_state_in_key: ctx->state = ini_state_in_key;
        case ini_state_in_key:
            DEBUG;
            // get everything before first punctuator
            tmp = mask0 & ((mask | (mask - 1)) ^ mask);

            at = (8*sizeof(tmp)) - __builtin_clz(tmp);
            ke = &ptr[at];

            if (mask == 0) {
                ptr += (len < 16) ? len : 16;
                len -= (len < 16) ? len : 16;
                continue;
            }

            at = __builtin_ctz(mask);

            if (ptr[at] != '=') {
                ptr += at + 1;
                len -= at + 1;
                // mask0 &= ~((1 << (at + 1)) - 1);
                // mask &= ~((1 << (at + 1)) - 1);
                continue;
            }

            mask0 &= ~((1 << (at + 1)) - 1);
            mask &= ~((1 << (at + 1)) - 1);
            goto _ini_state_begin_value;
        _ini_state_begin_value: ctx->state = ini_state_begin_value;
        case ini_state_begin_value:
            DEBUG;
            at = __builtin_ctz(mask0);
            vb = &ptr[at];
            goto _ini_state_in_value;
        _ini_state_in_value: ctx->state = ini_state_in_value;
        case ini_state_in_value:
            DEBUG;
            tmp = mask0 & ((mask | (mask - 1)) ^ mask);

            at = (8*sizeof(tmp)) - __builtin_clz(tmp);
            ve = &ptr[at];

            if (mask == 0) {
                ptr += (len < 16) ? len : 16;
                len -= (len < 16) ? len : 16;
                continue;
            }

            at = __builtin_ctz(mask);

            if (ptr[at] == '\n') {
                ctx->callback(sb, se - sb,
                              kb, ke - kb,
                              vb, ve - vb,
                              ctx->user);

                ctx->state = ini_state_begin_line;
                ptr += at + 1;
                len -= at + 1;
                continue;
            }

            // printf("error in file: %s\n", &ptr[at]);
            return false;
        }
    }
}

bool ini_parse_string(const char *s, size_t l, ini_callback_t callback, void *user)
{
    struct ini_ctx ctx;
    ctx.state = ini_state_begin_line;
    ctx.callback = callback;
    ctx.user = user;
    return ini_do(&ctx, s, l);
}

/*
int cb(const char *s, size_t sl, const char *k, size_t kl, const char *v, size_t vl)
{
    return 1;
    printf("[\"%.*s\"] \"%.*s\" = \"%.*s\"\n", sl, s, kl, k, vl, v);
}

int main()
{
    struct ini_state c;
    c.callback = &cb;
    c.state = ini_state_begin_line;

#if 1
    int fd = open("../../old/INI/test2.ini", O_RDONLY);

    struct stat st;
    fstat(fd, &st);

    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    df_do(&c, mem, st.st_size);
#else
    const char *str = "[a]b=c\n";
    df_do(&c, str, strlen(str));
#endif
}
*/
