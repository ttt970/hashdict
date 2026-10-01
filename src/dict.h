#ifndef HASHDICT_DICT_H
#define HASHDICT_DICT_H

#include "hash.h"
#include "store.h"

/* ------------------------------------------------------------------
 * dict —— 词典业务层
 * 负责把词典文件读进哈希表，并在其上提供精确/前缀/模糊/随机查询。
 * 上层(cli)只跟 dict 打交道，不用关心哈希表的细节。
 * ------------------------------------------------------------------ */

typedef struct {
    hashtable_t *ht;        /* 底层哈希表 */
    char        *source;    /* 词典文件路径 */
    double       load_ms;   /* 加载耗时（毫秒） */
    size_t       skipped;   /* 加载时跳过的空行/注释行数 */
    size_t       merged;     /* 被合并的续行数 */
} dict_t;

/* 加载词典文件，失败返回 NULL */
dict_t *dict_load(const char *path);
void    dict_free(dict_t *d);

/* 精确查询（大小写不敏感），未找到返回 NULL */
entry_t *dict_lookup(dict_t *d, const char *word);

/* 前缀查询：把匹配的单词按字典序写入 out（out 应是空的 strlist），
 * 返回匹配总数的（截断前的）数量。最多写入 limit 个 */
size_t dict_prefix(dict_t *d, const char *prefix, strlist_t *out, size_t limit);

/* 模糊查询的一条结果 */
typedef struct {
    entry_t *entry;
    int      dist;
} fuzzy_hit_t;

/* 模糊查询：找出与 word 编辑距离 <= max_dist 的单词，按距离升序、
 * 距离相同再按字典序，写入 *out（调用方负责 free）。返回结果条数 */
size_t dict_fuzzy(dict_t *d, const char *word, int max_dist,
                  fuzzy_hit_t **out, size_t limit);

/* 首字母分布统计（a-z 各有多少词条），打印成柱状图 */
void dict_letter_dist(const dict_t *d);

/* 随机返回一个词条（蓄水池抽样，各词等概率）*/
entry_t *dict_random(const dict_t *d);

/* 基准测试：对比"哈希查询"与"线性扫描"的单次平均耗时 */
void dict_benchmark(dict_t *d, const char *word);

#endif /* HASHDICT_DICT_H */
