#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- 跨平台的小适配：只在这一处 ifdef，其他文件保持干净 ---- */
#ifdef _WIN32
#  include <direct.h>
#  include <io.h>
#  define MKDIR_ONE(p)   _mkdir(p)
#  define ISATTY(fd)     _isatty(fd)
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>
#  define MKDIR_ONE(p)   mkdir((p), 0755)
#  define ISATTY(fd)     isatty(fd)
#endif

double util_now_ms(void)
{
#ifdef _WIN32
    /* Windows 上用标准 C 的 clock()，精度稍差但足够做相对比较 */
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}

char *util_strdup(const char *s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

char *util_trim(char *s)
{
    if (!s)
        return NULL;

    while (*s && isspace((unsigned char)*s))   /* 去头部空白 */
        s++;

    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))  /* 去尾部空白 */
        *--end = '\0';

    return s;
}

static int lower(int c)
{
    return tolower((unsigned char)c);
}

int util_streq_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (lower(*a) != lower(*b))
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int util_strneq_ci(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (lower(a[i]) != lower(b[i]))
            return 0;
        if (a[i] == '\0')          /* 两边同时到头也算相同 */
            return 1;
    }
    return 1;
}

int util_isatty_stdout(void)
{
    if (getenv("NO_COLOR") || getenv("HASHDICT_NO_COLOR"))
        return 0;
    return ISATTY(1);
}

int util_mkdir_p(const char *path)
{
    if (!path || !*path)
        return -1;

    char tmp[1024];
    size_t n = strlen(path);
    if (n >= sizeof(tmp))
        return -1;
    memcpy(tmp, path, n + 1);

    /* 逐个把路径分隔符临时改成 '\0'，逐级创建 */
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
            MKDIR_ONE(tmp);        /* 失败通常是因为已存在，忽略 */
            *p = save;
        }
    }
    MKDIR_ONE(tmp);
    return 0;
}

const char *util_home_dir(void)
{
    const char *h = getenv("HOME");
    if (!h || !*h)
        h = getenv("USERPROFILE");
    if (!h || !*h)
        h = ".";
    return h;
}
