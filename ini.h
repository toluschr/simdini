#ifndef _INI_H_
#define _INI_H_

#include <immintrin.h>
#include <stdbool.h>
#include <stddef.h>

typedef int (*ini_callback_t)(const char *s, size_t sl, const char *k, size_t kl, const char *v, size_t vl, void *user);

struct simdini {
    enum {
        ini_state_begin_line,
        ini_state_begin_section,
        ini_state_in_section,

        ini_state_in_key,
        ini_state_begin_value,
        ini_state_in_value,

        ini_state_in_comment,
    } state;

    ini_callback_t callback;
    size_t line;

    const char *sb;
    const char *se;

    const char *kb;
    const char *ke;

    const char *vb;
    const char *ve;

    const char *ptr;
    size_t len;

    void *user;
};

#ifdef __cplusplus
extern "C" {
#endif

extern bool ini_init(struct simdini *ctx, ini_callback_t callback, void *user);
extern bool ini_push(struct simdini *ctx, const char *str, size_t len);
extern bool ini_stop(struct simdini *ctx);

extern bool ini_parse_string(const char *s, size_t l, ini_callback_t callback, void *user);

#ifdef __cplusplus
}
#endif

#endif // _INI_H_
