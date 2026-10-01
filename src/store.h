#ifndef HASHDICT_STORE_H
#define HASHDICT_STORE_H

#include <stddef.h>

/* ------------------------------------------------------------------
 * strlist —— 顺序表（动态字符串数组）
 * 生词本、查询历史、前缀查询结果都用它。容量不够时按 2 倍扩容，
 * 这就是"顺序表"那套东西：连续内存 + 容量管理 + 均摊 O(1) 追加。
 * ------------------------------------------------------------------ */
typedef struct {
    char  **items;
    size_t  count;
    size_t  cap;
} strlist_t;

void strlist_init(strlist_t *l);
void strlist_free(strlist_t *l);

int  strlist_push(strlist_t *l, const char *s);          /* 追加，允许重复 */
int  strlist_push_unique(strlist_t *l, const char *s);   /* 去重追加（忽略大小写）*/
int  strlist_contains(const strlist_t *l, const char *s);/* 忽略大小写 */
int  strlist_remove(strlist_t *l, const char *s);        /* 删除首个匹配，返回是否删掉了 */
void strlist_sort(strlist_t *l);                         /* 字典序，忽略大小写 */

/* 持久化：每行一个词条，读入时自动去重、跳过空行与 '#' 注释行 */
int  strlist_load(strlist_t *l, const char *path);
int  strlist_save(const strlist_t *l, const char *path);

/* 数据目录 ~/.hashdict，返回其路径（首次调用会尝试创建）*/
const char *store_dir(void);

#endif /* HASHDICT_STORE_H */
