#include "fuzzy.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int lower(int c)
{
    return tolower((unsigned char)c);
}

/* 标准 Levenshtein DP，用两行滚动数组把空间从 O(m*n) 降到 O(min(m,n))。
 * 递推：d[i][j] = min(d[i-1][j]+1, d[i][j-1]+1, d[i-1][j-1]+cost)
 * cost = (a[i-1]==b[j-1]) ? 0 : 1 */
int fuzzy_distance_ci(const char *a, const char *b, int max_dist)
{
    int la = (int)strlen(a);
    int lb = (int)strlen(b);

    /* 长度差就已经超过上限，不可能满足，直接剪枝 */
    int diff = la > lb ? la - lb : lb - la;
    if (diff > max_dist)
        return max_dist + 1;

    /* 让 a 是较短的那个，滚动数组按短串开，省内存 */
    if (la > lb) {
        const char *t = a; a = b; b = t;
        int tn = la; la = lb; lb = tn;
    }

    int *prev = malloc((size_t)(la + 1) * sizeof(int));
    int *cur  = malloc((size_t)(la + 1) * sizeof(int));
    if (!prev || !cur) {
        free(prev);
        free(cur);
        return max_dist + 1;
    }

    for (int i = 0; i <= la; i++)
        prev[i] = i;

    for (int j = 1; j <= lb; j++) {
        cur[0] = j;
        int row_min = cur[0];              /* 本行最小值，用于提前剪枝 */

        for (int i = 1; i <= la; i++) {
            int cost = (lower(a[i - 1]) == lower(b[j - 1])) ? 0 : 1;

            int del = prev[i] + 1;
            int ins = cur[i - 1] + 1;
            int sub = prev[i - 1] + cost;

            int v = del < ins ? del : ins;
            if (sub < v)
                v = sub;

            cur[i] = v;
            if (v < row_min)
                row_min = v;
        }

        /* 整行都已经超过上限，再往下只会更大，直接放弃 */
        if (row_min > max_dist) {
            free(prev);
            free(cur);
            return max_dist + 1;
        }

        int *t = prev; prev = cur; cur = t;   /* 滚动 */
    }

    int result = prev[la];
    free(prev);
    free(cur);

    return result > max_dist ? max_dist + 1 : result;
}
