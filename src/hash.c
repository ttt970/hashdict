#include "hash.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 装载因子上限：count / nbuckets 超过它就把桶数翻倍并 rehash */
#define LOAD_FACTOR_MAX 0.75

/* ------------------------------------------------------------------
 * 哈希函数：BKDR Hash（种子 131）
 * 这是字符串哈希里分布较均匀、实现又极简单的一种。
 * 大小写不敏感：先统一转小写再算，这样 "Apple" 和 "apple" 落到同一个桶。
 * ------------------------------------------------------------------ */
static unsigned long hash_str(const char *s)
{
    unsigned long h = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c + 32);   /* to lower */
        h = h * 131u + c;
    }
    return h;
}

hashtable_t *ht_create(size_t nbuckets)
{
    hashtable_t *ht = calloc(1, sizeof(*ht));
    if (!ht)
        return NULL;

    if (nbuckets < 16)
        nbuckets = 16;

    ht->buckets = calloc(nbuckets, sizeof(entry_t *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    ht->nbuckets = nbuckets;
    return ht;
}

static void chain_free(entry_t *e)
{
    while (e) {
        entry_t *next = e->next;
        free(e->word);
        free(e->meaning);
        free(e);
        e = next;
    }
}

void ht_destroy(hashtable_t *ht)
{
    if (!ht)
        return;
    for (size_t i = 0; i < ht->nbuckets; i++)
        chain_free(ht->buckets[i]);
    free(ht->buckets);
    free(ht);
}

/* 重新统计非空桶数与最长链——迁移完成后调用 */
static void ht_recount(hashtable_t *ht)
{
    size_t used = 0, maxc = 0;
    for (size_t i = 0; i < ht->nbuckets; i++) {
        size_t len = 0;
        for (entry_t *e = ht->buckets[i]; e; e = e->next)
            len++;
        if (len) {
            used++;
            if (len > maxc)
                maxc = len;
        }
    }
    ht->used_buckets = used;
    ht->max_chain = maxc;
}

/* 扩容：新建更大的桶数组，把旧表所有结点重新挂过去（rehash）。
 * 注意这里只搬指针，不复制字符串，所以很快 */
static int ht_resize(hashtable_t *ht, size_t new_nbuckets)
{
    entry_t **nb = calloc(new_nbuckets, sizeof(entry_t *));
    if (!nb)
        return -1;

    for (size_t i = 0; i < ht->nbuckets; i++) {
        entry_t *e = ht->buckets[i];
        while (e) {
            entry_t *next = e->next;                 /* 先存 next，防止断链 */
            size_t idx = hash_str(e->word) % new_nbuckets;
            e->next = nb[idx];
            nb[idx] = e;
            e = next;
        }
    }

    free(ht->buckets);
    ht->buckets = nb;
    ht->nbuckets = new_nbuckets;
    ht_recount(ht);
    return 0;
}

entry_t *ht_insert(hashtable_t *ht, const char *word, const char *meaning)
{
    if (!ht || !word || !meaning)
        return NULL;

    size_t idx = hash_str(word) % ht->nbuckets;

    /* 已存在则更新释义 */
    for (entry_t *e = ht->buckets[idx]; e; e = e->next) {
        if (util_streq_ci(e->word, word)) {
            char *m = util_strdup(meaning);
            if (m) {
                free(e->meaning);
                e->meaning = m;
            }
            return e;
        }
    }

    /* 新建结点，头插进冲突链 */
    entry_t *e = malloc(sizeof(*e));
    if (!e)
        return NULL;
    e->word = util_strdup(word);
    e->meaning = util_strdup(meaning);
    if (!e->word || !e->meaning) {
        free(e->word);
        free(e->meaning);
        free(e);
        return NULL;
    }
    e->next = ht->buckets[idx];
    ht->buckets[idx] = e;
    ht->count++;

    /* 维护统计量 */
    if (!e->next)
        ht->used_buckets++;
    size_t len = 0;
    for (entry_t *p = ht->buckets[idx]; p; p = p->next)
        len++;
    if (len > ht->max_chain)
        ht->max_chain = len;

    /* 装载因子过高就扩容 */
    if ((double)ht->count / (double)ht->nbuckets > LOAD_FACTOR_MAX)
        ht_resize(ht, ht->nbuckets * 2);

    return e;
}

entry_t *ht_find(hashtable_t *ht, const char *word)
{
    if (!ht || !word)
        return NULL;

    size_t idx = hash_str(word) % ht->nbuckets;
    for (entry_t *e = ht->buckets[idx]; e; e = e->next)
        if (util_streq_ci(e->word, word))
            return e;

    return NULL;
}

void ht_foreach(hashtable_t *ht, ht_visit_fn cb, void *arg)
{
    if (!ht || !cb)
        return;
    for (size_t i = 0; i < ht->nbuckets; i++)
        for (entry_t *e = ht->buckets[i]; e; e = e->next)
            if (cb(e, arg) != 0)
                return;
}

entry_t **ht_to_array(hashtable_t *ht, size_t *out_n)
{
    if (!ht) {
        if (out_n)
            *out_n = 0;
        return NULL;
    }

    entry_t **arr = malloc(ht->count * sizeof(entry_t *));
    if (!arr) {
        if (out_n)
            *out_n = 0;
        return NULL;
    }

    size_t n = 0;
    for (size_t i = 0; i < ht->nbuckets; i++)
        for (entry_t *e = ht->buckets[i]; e; e = e->next)
            arr[n++] = e;

    if (out_n)
        *out_n = n;
    return arr;
}

void ht_print_stats(const hashtable_t *ht)
{
    if (!ht)
        return;

    double lf = ht->nbuckets ? (double)ht->count / (double)ht->nbuckets : 0.0;
    double used_pct = ht->nbuckets ? (double)ht->used_buckets / (double)ht->nbuckets * 100.0 : 0.0;
    double avg_chain = ht->used_buckets ? (double)ht->count / (double)ht->used_buckets : 0.0;

    printf("  单词总数    : %zu\n", ht->count);
    printf("  桶数量      : %zu\n", ht->nbuckets);
    printf("  装载因子    : %.3f  (上限 %.2f，超过自动扩容)\n", lf, LOAD_FACTOR_MAX);
    printf("  非空桶数    : %zu  (%.1f%%)\n", ht->used_buckets, used_pct);
    printf("  最长冲突链  : %zu\n", ht->max_chain);
    printf("  非空桶平均链长: %.2f\n", avg_chain);
}
