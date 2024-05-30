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

#define fallthrough __attribute__((fallthrough))

#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x) __builtin_expect(!!(x), 1)

#if 0
#define DEBUG(...) { \
        fprintf(stderr, "%s:%d -> %s:\n", __FILE__, __LINE__, __ini_str_state(ctx->state)); \
        __ini_print(__VA_ARGS__); \
    }

static const char *__ini_str_state(int s)
{
    switch (s) {
    case ini_state_begin_line: return "begin_line";
    case ini_state_begin_section: return "begin_section";
    case ini_state_in_section: return "in_section";
    case ini_state_in_key: return "in_key";
    case ini_state_begin_value: return "begin_value";
    case ini_state_in_value: return "in_value";
    case ini_state_in_comment: return "in_comment";
    default: return "invalid";
    }
}

static void __ini_print(__m256i d, uint32_t arrow)
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

    for (size_t i = 0; i < sizeof(arr) / sizeof(*arr); i++) {
        char str[5];

        if (' ' <= arr[i] && arr[i] <= '~') {
            str[0] = ' ';
            str[1] = ' ';
            str[2] = ' ';
            str[3] = arr[i];
            str[4] = 0;
        } else {
            str[0] = '\\';
            str[1] = '0' + (((arr[i] / 8) / 8) % 8);
            str[2] = '0' + ((arr[i] / 8) % 8);
            str[3] = '0' + ((arr[i]) % 8);
            str[4] = '\0';
        }

        fprintf(stderr, "\t[%2lu] = '%s'", i, str);

        if ((arrow >> i) & 1) {
            fprintf(stderr, " \033[1m<---\033[0m");
        }

        fprintf(stderr, "\n");
    }
}
#else
#define DEBUG(...)
#endif

static __m256i ini_load(const char *ptr, uint64_t len)
{
    __m256i out; // uninit safe: only the ignored half is uninitialized
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
        a = _mm_insert_epi8(a, ptr[16 * (len / 16) + (i - 1)], 0);
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
    __m256i space = _mm256_set1_epi8(' ');
    __m256i line_feed = _mm256_set1_epi8('\n');
    __m256i right_square_bracket = _mm256_set1_epi8(']');
    __m256i equal = _mm256_set1_epi8('=');

    const char *se = NULL, *sb = NULL;
    const char *ke = NULL, *kb = NULL;
    const char *ve = NULL, *vb = NULL;

    const char *np = ptr;
    size_t nl = len;

    for (;;) {
        __m256i d = ini_load(np, nl);

        ptr = np;
        len = nl;

        np += (nl < 32) ? nl : 32;
        nl -= (nl < 32) ? nl : 32;

        __m256i eq_non_space = _mm256_cmpeq_epi8(d, space);
        __m256i eq_rsb = _mm256_cmpeq_epi8(d, right_square_bracket);
        __m256i eq_equal = _mm256_cmpeq_epi8(d, equal);
        __m256i eq_line_feed = _mm256_cmpeq_epi8(d, line_feed);

        int mask_non_space = ~_mm256_movemask_epi8(eq_non_space);
        int mask_rsb = _mm256_movemask_epi8(eq_rsb);
        int mask_equal = _mm256_movemask_epi8(eq_equal);
        int mask_line_feed = _mm256_movemask_epi8(eq_line_feed);

        switch (ctx->state) {
        int at, tmp;

        _ini_state_begin_line: ctx->state = ini_state_begin_line; fallthrough;
        case ini_state_begin_line:
            if (len == 0) return true;

            DEBUG(d, mask_non_space);

            if (mask_non_space == 0) continue;

            at = __builtin_ctz(mask_non_space);
            switch (ptr[at]) {
            case '[':
                mask_non_space &= ~((2 << at) - 1);
                goto _ini_state_begin_section;
            case '\n':
                mask_non_space &= ~((2 << at) - 1);
                mask_line_feed &= mask_non_space;
                goto _ini_state_begin_line;
            case '#':
                mask_non_space &= ~((2 << at) - 1);
                goto _ini_state_in_comment;
            // hack
            case '\0':
                return true;
            default:
                if (!isalpha(ptr[at])) {
                    return false;
                }

                kb = &ptr[at];
                ke = kb;
                goto _ini_state_in_key;
            }
            break;
        _ini_state_begin_section: ctx->state = ini_state_begin_section; fallthrough;
        case ini_state_begin_section:
            if (len == 0) return false;

            DEBUG(d, mask_non_space);
            if (mask_non_space == 0) continue;

            at = __builtin_ctz(mask_non_space);
            sb = &ptr[at];
            se = sb;
            goto _ini_state_in_section;
        _ini_state_in_section: ctx->state = ini_state_in_section; fallthrough;
        case ini_state_in_section:
            if (len == 0) return false;

            tmp = mask_non_space & ((mask_rsb | (mask_rsb - 1)) ^ mask_rsb);
            DEBUG(d, tmp);

            if (tmp != 0) {
                at = (8*sizeof(tmp)) - __builtin_clz(tmp);
                se = &ptr[at];
            }

            if (mask_rsb == 0) continue;

            at = __builtin_ctz(mask_rsb);
            mask_non_space &= ~((2 << at) - 1);
            mask_rsb &= mask_non_space;
            goto _ini_state_begin_line;
        _ini_state_in_key: ctx->state = ini_state_in_key; fallthrough;
        case ini_state_in_key:
            if (len == 0) return true;

            tmp = mask_non_space & ((mask_equal | (mask_equal - 1)) ^ mask_equal);
            DEBUG(d, tmp);

            if (tmp != 0) {
                at = (8*sizeof(tmp)) - __builtin_clz(tmp);
                ke = &ptr[at];
            }

            if (mask_equal == 0) continue;

            at = __builtin_ctz(mask_equal);
            mask_non_space &= ~((2 << at) - 1);
            mask_equal &= mask_non_space;
            goto _ini_state_begin_value;
        _ini_state_begin_value: ctx->state = ini_state_begin_value; fallthrough;
        case ini_state_begin_value:
            if (len == 0) return true;

            DEBUG(d, mask_non_space);

            if (mask_non_space == 0) continue;

            at = __builtin_ctz(mask_non_space);
            vb = &ptr[at];
            ve = vb;
            goto _ini_state_in_value;
        _ini_state_in_value: ctx->state = ini_state_in_value; fallthrough;
        case ini_state_in_value:
            if (len == 0) {
                ctx->callback(sb, se - sb, kb, ke - kb, vb, &ptr[len] - vb, ctx->user);
                return true;
            }

            tmp = mask_non_space & ((mask_line_feed | (mask_line_feed - 1)) ^ mask_line_feed);
            DEBUG(d, tmp);

            if (tmp != 0) {
                at = (8*sizeof(tmp)) - __builtin_clz(tmp);
                ve = &ptr[at];
            }

            if (mask_line_feed == 0) continue;

            ctx->callback(sb, se - sb, kb, ke - kb, vb, ve - vb, ctx->user);

            at = __builtin_ctz(mask_line_feed);
            mask_non_space &= ~((2 << at) - 1);
            mask_line_feed &= mask_non_space;
            goto _ini_state_begin_line;
        _ini_state_in_comment: ctx->state = ini_state_in_comment; fallthrough;
        case ini_state_in_comment:
            if (len == 0) return true;

            DEBUG(d, mask_line_feed);

            if (mask_line_feed == 0) continue;

            at = __builtin_ctz(mask_line_feed);
            mask_non_space &= ~((2 << at) - 1);
            mask_line_feed &= mask_non_space;
            goto _ini_state_begin_line;
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
