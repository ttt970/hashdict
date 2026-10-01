#ifndef HASHDICT_HASH_H
#define HASHDICT_HASH_H

#include <stddef.h>

/* ------------------------------------------------------------------
 * 哈希表 —— 链地址法（separate chaining）
 *
 *   桶数组 buckets[nbuckets]，每个桶挂一条单链表存冲突的单词。
 *   查询 O(1) 期望；装载因子超过阈值时自动扩容并 rehash，保证链表不退化。
 *   相比老师原版"每次查询线性扫描整个文件"的 O(n)，这是性能上的关键改进。
 * ------------------------------------------------------------------ */

typedef struct entry {
    char         *word;     /* 单词（堆分配） */
    char         *meaning;  /* 释义（堆分配） */
    struct entry *next;     /* 冲突链的下一个结点 */
} entry_t;

typedef struct {
    entry_t **buckets;      /* 桶数组，每个元素是一条链表的头指针 */
    size_t    nbuckets;     /* 桶的数量 */
    size_t    count;        /* 已存单词总数 */
    size_t    used_buckets; /* 非空桶数量 */
    size_t    max_chain;    /* 最长冲突链长度（衡量哈希是否均匀） */
} hashtable_t;

/* 创建哈希表；nbuckets 过小时会被提升到 16 */
hashtable_t *ht_create(size_t nbuckets);

/* 销毁整张表，释放所有内存 */
void ht_destroy(hashtable_t *ht);

/* 插入单词。若已存在（大小写不敏感）则更新释义并返回原结点；
 * 否则新建结点返回。失败返回 NULL */
entry_t *ht_insert(hashtable_t *ht, const char *word, const char *meaning);

/* 精确查找（大小写不敏感），未找到返回 NULL */
entry_t *ht_find(hashtable_t *ht, const char *word);

/* 遍历所有结点。cb 返回非 0 时提前结束 */
typedef int (*ht_visit_fn)(entry_t *e, void *arg);
void ht_foreach(hashtable_t *ht, ht_visit_fn cb, void *arg);

/* 把所有结点指针收集进一个新数组（调用方负责 free 数组本身），
 * 返回数组长度。用于排序输出、基准测试里的线性扫描 */
entry_t **ht_to_array(hashtable_t *ht, size_t *out_n);

/* 打印哈希表的统计信息（桶数、装载因子、最长链等） */
void ht_print_stats(const hashtable_t *ht);

#endif /* HASHDICT_HASH_H */
