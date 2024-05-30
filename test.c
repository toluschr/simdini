#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include "ini.h"

struct ini_test {
    const char *string;
    int rc;
    const char *(expect[])[3];
};

static struct ini_test empty = {
    "",
    true,
    {
        {NULL, NULL, NULL}
    }
};

static struct ini_test empty_section_name_only = {
    "[]",
    true,
    {
        {NULL, NULL, NULL}
    }
};

static struct ini_test section_name_only = {
    "[a]",
    true,
    {
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_no_newline_at_eof = {
    "[a]\nb=c",
    true,
    {
        {"a", "b", "c"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_newline_at_eof = {
    "[a]\nb=c\n",
    true,
    {
        {"a", "b", "c"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_space_at_beginning_of_line = {
    " [a]\nb=c\n",
    true,
    {
        {"a", "b", "c"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_section_at_32byte_boundary = {
    "                               [aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa]\nb=c\n",
    true,
    {
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "b", "c"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_empty_section = {
    "[a]\n\n\n[b]\nc=d\n",
    true,
    {
        {"b", "c", "d"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_hashtag_in_value = {
    "[a]\n\n\n[b]\nc=#d\n",
    true,
    {
        {"b", "c", "#d"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_hashtag_after_section = {
    "[a]#\n[b]\nc=#d\n",
    true,
    {
        {"b", "c", "#d"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_unterminated_section = {
    "[a",
    false,
    {
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_empty_value = {
    "[a]\nb=\n",
    true,
    {
        {"a", "b", ""},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_empty_value_no_newline_at_eof = {
    "[a]\nb=",
    true,
    {
        {"a", "b", ""},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_empty_key = {
    "[a]\n=b",
    false,
    {
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_empty_section_name = {
    "[]\na=b",
    true,
    {
        {"", "a", "b"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_overlong_names = {
    "[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa]\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    true,
    {
        {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
        {NULL, NULL, NULL},
    }
};

static struct ini_test ini_key_at_32byte_boundary = {
    "[]\n                             aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=b",
    true,
    {
        {"", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "b"},
        {NULL, NULL, NULL}
    }
};

static struct ini_test ini_value_at_32byte_boundary = {
    "[]\na=                           bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    true,
    {
        {"", "a", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
        {NULL, NULL, NULL}
    }
};

typedef struct {
    const char *name;
    struct ini_test *test;
} ini_testsuite[];

#define TEST(ID) {#ID, &ID}

ini_testsuite all_tests = {
    TEST(empty),
    TEST(empty_section_name_only),
    TEST(section_name_only),
    TEST(ini_no_newline_at_eof),
    TEST(ini_newline_at_eof),
    TEST(ini_space_at_beginning_of_line),
    TEST(ini_section_at_32byte_boundary),
    TEST(ini_empty_section),
    TEST(ini_hashtag_in_value),
    TEST(ini_hashtag_after_section),
    TEST(ini_unterminated_section),
    TEST(ini_empty_value),
    TEST(ini_empty_value_no_newline_at_eof),
    TEST(ini_empty_key),
    TEST(ini_empty_section_name),
    TEST(ini_overlong_names),
    TEST(ini_key_at_32byte_boundary),
    TEST(ini_value_at_32byte_boundary),
    {NULL, NULL},
};

static bool ok;

static int check_callback(const char *s, size_t sl, const char *k, size_t kl, const char *v, size_t vl, void *user) {
    (void)(user);
    const char *(**expect)[3] = user;

    const char *es = (**expect)[0];
    const char *ek = (**expect)[1];
    const char *ev = (**expect)[2];

    if (!ok) {
        return 1;
    }

    // end marker
    if (es == NULL && ek == NULL && ev == NULL) {
        ok = false;
        // fprintf(stderr, "Additional data\n");
        return 1;
    }

    size_t esl = strlen(es);
    size_t ekl = strlen(ek);
    size_t evl = strlen(ev);

    ok &= (esl == sl && memcmp(s, es, esl) == 0);
    ok &= (ekl == kl && memcmp(k, ek, ekl) == 0);
    ok &= (evl == vl && memcmp(v, ev, evl) == 0);

    (*expect)++;
    return 1;
}

static int print_callback(const char *s, size_t sl, const char *k, size_t kl, const char *v, size_t vl, void *user) {
    (void)(user);

    return 1;
    printf("[%.*s] %.*s = %.*s\n", (int)sl, s, (int)kl, k, (int)vl, v);
}

int main(int argc, char **argv)
{
    int rc = EXIT_SUCCESS;

    switch (argc) {
    default:
        fprintf(stderr, "usage: test <name>\n");
        exit(EXIT_FAILURE);
    case 1:
        for (int i = 0; all_tests[i].name != NULL; i++) {
            ok = true;

            fprintf(stderr, "[    ] %s\r", all_tests[i].name);
            const char *(*check_data)[3] = all_tests[i].test->expect;
            if (ini_parse_string(all_tests[i].test->string, strlen(all_tests[i].test->string), &check_callback, &check_data) != all_tests[i].test->rc) {
                ok = false;
                // fprintf(stderr, "Unexpected return value\n");
            }

            if ((*check_data)[0] != NULL || (*check_data)[1] != NULL || (*check_data)[2] != NULL) {
                // fprintf(stderr, "Unexpected end of file\n");
                ok = false;
            }

            if (ok) {
                fprintf(stderr, "\033[32;1m✔\033[0m %s\033[K\n", all_tests[i].name);
            } else {
                fprintf(stderr, "\033[31;1m✘\033[0m %s\033[K\n", all_tests[i].name);
                rc = EXIT_FAILURE;
            }
        }
        break;
    case 2:
#if 1
        char buf[4096 + 1];
        ssize_t actual_size = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (actual_size < 0) {
            perror("readlink failed");
            exit(EXIT_FAILURE);
        }
        buf[actual_size] = 0;

        char *at = strrchr(buf, '/');
        if (at) *at = '\0';

        int dirfd = open(buf, O_RDONLY);
        if (dirfd < 0) {
            perror("open test dir failed");
            exit(EXIT_FAILURE);
        }

        int fd = openat(dirfd, argv[1], O_RDONLY);
        close(dirfd);
#else
        int fd = open(argv[1], O_RDONLY);
#endif
        if (fd < 0) {
            perror("open failed");
            exit(EXIT_FAILURE);
        }

        struct stat st;
        if (fstat(fd, &st) < 0) {
            perror("stat failed");
            close(fd);
            exit(EXIT_FAILURE);
        }

        void *addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (addr == MAP_FAILED) {
            perror("mmap failed");
            exit(EXIT_FAILURE);
        }

        ini_parse_string(addr, st.st_size, print_callback, NULL);

        if (munmap(addr, st.st_size) < 0) {
            perror("munmap failed");
            exit(EXIT_FAILURE);
        }
        break;
    }

    return rc;
}
