#include "dict.h"
#include "fuzzy.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 初始桶数：取 2^14=16384。加载过程中装载因子超过 0.75 会自动翻倍，
 * 最终 19659 个词大概会稳定在 32768 个桶，装载因子约 0.6 */
#define DICT_INIT_BUCKETS (1u << 14)

/* 单行最大长度。词典最长的释义 271 字符，4096 足够；更长的行会被截断，
 * 实际数据里不会发生 */
#define LINE_MAX_LEN 4096

dict_t *dict_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "无法打开词典文件: %s\n", path);
        return NULL;
    }

    dict_t *d = calloc(1, sizeof(*d));
    if (!d) {
        fclose(fp);
        return NULL;
    }

    d->ht = ht_create(DICT_INIT_BUCKETS);
    d->source = util_strdup(path);
    if (!d->ht || !d->source) {
        dict_free(d);
        fclose(fp);
        return NULL;
    }

    char   line[LINE_MAX_LEN];
    char   last_word[256] = {0};   /* 最近一个成功插入的单词，用于处理续行 */
    size_t lineno = 0;
    double t0 = util_now_ms();

    while (fgets(line, sizeof(line), fp)) {
        lineno++;

        /* 关键点 1：续行处理。
         * 词典里极少数释义会折到下一行，表现为"以空白开头的行"。
         * 例如 overburden 的释义被拆成两行，这里要把它拼回上一个单词 */
        if ((line[0] == ' ' || line[0] == '\t') && last_word[0]) {
            char *cont = util_trim(line);
            if (*cont) {
                entry_t *e = ht_find(d->ht, last_word);
                if (e) {
                    size_t on = strlen(e->meaning);
                    size_t cn = strlen(cont);
                    char  *nm = malloc(on + cn + 2);
                    if (nm) {
                        snprintf(nm, on + cn + 2, "%s %s", e->meaning, cont);
                        free(e->meaning);
                        e->meaning = nm;
                        d->merged++;
                    }
                }
            }
            continue;
        }

        /* 关键点 2：行尾是 CRLF，util_trim 会一并去掉 '\r'，
         * 否则每个单词/释义后面都会粘一个不可见字符，导致查不到 */
        char *p = util_trim(line);
        if (*p == '\0' || *p == '#') {
            d->skipped++;
            continue;
        }

        /* 拆分：第一个"连续空白"之前是单词，之后是释义。
         * 用这个规则而不是 strtok，是因为 strtok 会破坏原串且跳过连续分隔符 */
        char *sep = p;
        while (*sep && *sep != ' ' && *sep != '\t')
            sep++;

        char *word, *mean;
        if (*sep == '\0') {
            word = p;          /* 整行就是一个单词，没有释义 */
            mean = sep;
        } else {
            *sep = '\0';
            word = p;
            mean = util_trim(sep + 1);
        }

        if (*word == '\0')
            continue;

        ht_insert(d->ht, word, mean);
        snprintf(last_word, sizeof(last_word), "%s", word);

        /* 加载进度，每 2000 行刷新一次 */
        if (util_isatty_stdout() && (lineno % 2000 == 0)) {
            printf("\r  加载中... %zu 行", lineno);
            fflush(stdout);
        }
    }

    d->load_ms = util_now_ms() - t0;

    if (util_isatty_stdout() && lineno >= 2000)
        printf("\r\033[K");   /* 清掉进度行 */

    fclose(fp);
    return d;
}

void dict_free(dict_t *d)
{
    if (!d)
        return;
    ht_destroy(d->ht);
    free(d->source);
    free(d);
}

entry_t *dict_lookup(dict_t *d, const char *word)
{
    return d ? ht_find(d->ht, word) : NULL;
}

size_t dict_prefix(dict_t *d, const char *prefix, strlist_t *out, size_t limit)
{
    if (!d || !prefix || !*prefix)
        return 0;

    size_t plen = strlen(prefix);
    size_t total = 0;

    /* 先全部收集（前缀匹配无法靠哈希直接定位，只能扫一遍），
     * 排序后再截断，保证给出的是字典序最靠前的 limit 个 */
    for (size_t i = 0; i < d->ht->nbuckets; i++) {
        for (entry_t *e = d->ht->buckets[i]; e; e = e->next) {
            if (util_strneq_ci(e->word, prefix, plen)) {
                total++;
                if (out->count < limit)
                    strlist_push(out, e->word);
            }
        }
    }

    strlist_sort(out);
    return total;
}

/* 给 qsort 用：先按编辑距离升序，距离相同按字典序 */
static int cmp_fuzzy(const void *a, const void *b)
{
    const fuzzy_hit_t *ha = a;
    const fuzzy_hit_t *hb = b;
    if (ha->dist != hb->dist)
        return ha->dist - hb->dist;
#ifdef _WIN32
    return _stricmp(ha->entry->word, hb->entry->word);
#else
    return strcasecmp(ha->entry->word, hb->entry->word);
#endif
}

size_t dict_fuzzy(dict_t *d, const char *word, int max_dist,
                  fuzzy_hit_t **out, size_t limit)
{
    *out = NULL;
    if (!d || !word || !*word)
        return 0;

    /* 先扫一遍哈希表，收集所有距离在阈值内的候选。
     * 编辑距离带剪枝，所以即使 2 万个词也不会慢 */
    fuzzy_hit_t *buf = malloc(d->ht->count * sizeof(fuzzy_hit_t));
    if (!buf)
        return 0;

    size_t n = 0;
    for (size_t i = 0; i < d->ht->nbuckets; i++) {
        for (entry_t *e = d->ht->buckets[i]; e; e = e->next) {
            int dist = fuzzy_distance_ci(word, e->word, max_dist);
            if (dist <= max_dist) {
                buf[n].entry = e;
                buf[n].dist = dist;
                n++;
            }
        }
    }

    qsort(buf, n, sizeof(fuzzy_hit_t), cmp_fuzzy);

    if (n > limit)
        n = limit;
    *out = buf;              /* 多余的槽位留着，调用方整个 free 即可 */
    return n;
}

/* ---- 字母分布统计 ---- */
static int count_letter(entry_t *e, void *arg)
{
    size_t *cnt = arg;
    unsigned char c = (unsigned char)e->word[0];
    if (c >= 'A' && c <= 'Z')
        c = (unsigned char)(c + 32);
    if (c >= 'a' && c <= 'z')
        cnt[c - 'a']++;
    else
        cnt[26]++;          /* 非字母开头 */
    return 0;
}

void dict_letter_dist(const dict_t *d)
{
    if (!d)
        return;

    size_t cnt[27] = {0};
    ht_foreach((hashtable_t *)d->ht, count_letter, cnt);

    size_t maxv = 1;
    for (int i = 0; i < 27; i++)
        if (cnt[i] > maxv)
            maxv = cnt[i];

    printf("首字母分布（共 %zu 词条）:\n", d->ht->count);
    for (int i = 0; i < 26; i++) {
        printf("  %c |", 'a' + i);
        int bar = (int)((double)cnt[i] / (double)maxv * 40.0);
        for (int j = 0; j < bar; j++)
            putchar('#');
        printf(" %zu\n", cnt[i]);
    }
    if (cnt[26])
        printf("  * | %zu (非字母开头)\n", cnt[26]);
}

/* ---- 随机单词：蓄水池抽样，O(n) 一遍且各词等概率 ---- */
static int rand_reservoir(entry_t *e, void *arg)
{
    struct {
        entry_t *pick;
        size_t   seen;
    } *st = arg;

    st->seen++;
    if (rand() % (int)st->seen == 0)
        st->pick = e;
    return 0;
}

entry_t *dict_random(const dict_t *d)
{
    if (!d || d->ht->count == 0)
        return NULL;

    static int seeded = 0;
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }

    struct {
        entry_t *pick;
        size_t   seen;
    } st = {NULL, 0};

    ht_foreach((hashtable_t *)d->ht, rand_reservoir, &st);
    return st.pick;
}

/* ---- 基准测试：哈希查询 vs 线性扫描 ---- */
#define BENCH_HASH_ITERS 200000
#define BENCH_LIN_ITERS  200

void dict_benchmark(dict_t *d, const char *word)
{
    if (!d || !word)
        return;

    size_t n = 0;
    entry_t **arr = ht_to_array(d->ht, &n);
    if (!arr) {
        printf("基准测试内存不足\n");
        return;
    }

    /* volatile 防止编译器把"没用到结果"的循环整个优化掉 */
    volatile size_t sink = 0;

    /* 1) 哈希查询 */
    double t0 = util_now_ms();
    for (int i = 0; i < BENCH_HASH_ITERS; i++) {
        entry_t *e = ht_find(d->ht, word);
        if (e)
            sink++;
    }
    double t1 = util_now_ms();

    /* 2) 线性扫描（等价于老师原版每次从头扫文件的思路）*/
    for (int i = 0; i < BENCH_LIN_ITERS; i++) {
        for (size_t j = 0; j < n; j++) {
            if (util_streq_ci(arr[j]->word, word)) {
                sink++;
                break;
            }
        }
    }
    double t2 = util_now_ms();

    free(arr);
    (void)sink;

    double hash_avg = (t1 - t0) * 1e6 / BENCH_HASH_ITERS;   /* 每次纳秒 */
    double lin_avg  = (t2 - t1) * 1e6 / BENCH_LIN_ITERS;

    printf("基准测试  目标单词: \"%s\"  (词条数 %zu)\n", word, d->ht->count);
    printf("  哈希表查询 : %.0f 次/秒区间  平均 %.1f ns/次  (%d 次共 %.1f ms)\n",
           BENCH_HASH_ITERS / (t1 - t0) * 1000.0, hash_avg, BENCH_HASH_ITERS, t1 - t0);
    printf("  线性扫描   : 平均 %.1f ns/次  (%d 次共 %.1f ms)\n",
           lin_avg, BENCH_LIN_ITERS, t2 - t1);
    if (hash_avg > 0)
        printf("  加速比     : 约 %.0f 倍\n", lin_avg / hash_avg);
}
