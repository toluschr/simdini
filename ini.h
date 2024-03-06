#ifndef _INI_H_
#define _INI_H_

#include <immintrin.h>
#include <stdbool.h>
#include <stddef.h>

typedef int (*ini_callback_t)(const char *s, size_t sl, const char *k, size_t kl, const char *v, size_t vl, void *user);

struct ini_ctx {
    enum {
        ini_state_begin_line,
        ini_state_begin_section,
        ini_state_in_section,

        ini_state_in_key,
        ini_state_begin_value,
        ini_state_in_value,
    } state;

    ini_callback_t callback;
    void *user;
    __m256i t0;
};

bool ini_parse_string(const char *s, size_t l, ini_callback_t callback, void *user);

#endif // _INI_H_
