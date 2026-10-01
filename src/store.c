#include "store.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void strlist_init(strlist_t *l)
{
    l->items = NULL;
    l->count = 0;
    l->cap = 0;
}

void strlist_free(strlist_t *l)
{
    if (!l)
        return;
    for (size_t i = 0; i < l->count; i++)
        free(l->items[i]);
    free(l->items);
    l->items = NULL;
    l->count = 0;
    l->cap = 0;
}

/* 保证至少能放下 need 个元素，容量按 2 倍增长（均摊 O(1)）*/
static int strlist_reserve(strlist_t *l, size_t need)
{
    if (need <= l->cap)
        return 0;

    size_t cap = l->cap ? l->cap * 2 : 16;
    while (cap < need)
        cap *= 2;

    char **ni = realloc(l->items, cap * sizeof(char *));
    if (!ni)
        return -1;

    l->items = ni;
    l->cap = cap;
    return 0;
}

int strlist_push(strlist_t *l, const char *s)
{
    if (strlist_reserve(l, l->count + 1) != 0)
        return -1;

    char *cp = util_strdup(s);
    if (!cp)
        return -1;

    l->items[l->count++] = cp;
    return 0;
}

int strlist_contains(const strlist_t *l, const char *s)
{
    for (size_t i = 0; i < l->count; i++)
        if (util_streq_ci(l->items[i], s))
            return 1;
    return 0;
}

int strlist_push_unique(strlist_t *l, const char *s)
{
    if (strlist_contains(l, s))
        return 0;
    return strlist_push(l, s);
}

int strlist_remove(strlist_t *l, const char *s)
{
    for (size_t i = 0; i < l->count; i++) {
        if (util_streq_ci(l->items[i], s)) {
            free(l->items[i]);
            /* 后面整体前移一位 */
            memmove(&l->items[i], &l->items[i + 1],
                    (l->count - i - 1) * sizeof(char *));
            l->count--;
            return 1;
        }
    }
    return 0;
}

static int cmp_ci(const void *a, const void *b)
{
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
#ifdef _WIN32
    return _stricmp(sa, sb);
#else
    return strcasecmp(sa, sb);
#endif
}

void strlist_sort(strlist_t *l)
{
    if (l->count > 1)
        qsort(l->items, l->count, sizeof(char *), cmp_ci);
}

int strlist_load(strlist_t *l, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;                 /* 文件不存在是正常情况，返回 -1 但不报错 */

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *p = util_trim(line);
        if (*p == '\0' || *p == '#')
            continue;
        strlist_push_unique(l, p);
    }

    fclose(fp);
    return 0;
}

int strlist_save(const strlist_t *l, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp)
        return -1;

    for (size_t i = 0; i < l->count; i++)
        fprintf(fp, "%s\n", l->items[i]);

    fclose(fp);
    return 0;
}

const char *store_dir(void)
{
    static char dir[1024];
    static int  inited = 0;

    if (!inited) {
        snprintf(dir, sizeof(dir), "%s/.hashdict", util_home_dir());
        util_mkdir_p(dir);
        inited = 1;
    }
    return dir;
}
