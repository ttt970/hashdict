#include "cli.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- ANSI 颜色。
 * 用变量而不是宏，是因为宏做三选一后就不是字符串字面量了，没法用 "A" "B"
 * 那种字面量拼接；变量则可以直接当 %s 参数传。
 * 非终端（重定向到文件）时会全部置空串，避免日志里出现转义码 */
static const char *RESET   = "\033[0m";
static const char *BOLD    = "\033[1m";
static const char *DIM     = "\033[2m";
static const char *CYAN    = "\033[36m";
static const char *GREEN   = "\033[32m";
static const char *YELLOW  = "\033[33m";
static const char *MAGENTA = "\033[35m";

static void colors_init(void)
{
    if (util_isatty_stdout())
        return;                 /* 是终端，保留颜色 */

    RESET = BOLD = DIM = CYAN = GREEN = YELLOW = MAGENTA = "";
}

/* 模糊查询默认最多列几条建议 */
#define FUZZY_LIMIT 5
/* 前缀查询默认最多列几条 */
#define PREFIX_LIMIT 40

static void show_entry(const entry_t *e)
{
    printf("%s%s%s\n", BOLD, CYAN, e->word);
    printf("%s", RESET);
    if (e->meaning && *e->meaning)
        printf("  %s\n", e->meaning);
    else
        printf("  %s(无释义)%s\n", DIM, RESET);
}

/* 对没查到的词给出"你是不是想查"的近似建议 */
static void suggest(dict_t *d, const char *word)
{
    /* 短词允许的距离收小一点，否则会冒出一堆无关词 */
    int max_dist = (strlen(word) <= 4) ? 1 : 2;

    fuzzy_hit_t *hits = NULL;
    size_t n = dict_fuzzy(d, word, max_dist, &hits, FUZZY_LIMIT);

    if (n == 0) {
        printf("%s未找到 \"%s\"%s\n", YELLOW, word, RESET);
        free(hits);
        return;
    }

    printf("%s未找到 \"%s\"，你是不是想查：%s\n", YELLOW, word, RESET);
    for (size_t i = 0; i < n; i++)
        printf("  %s%s%s  %s(编辑距离 %d)%s\n",
               GREEN, hits[i].entry->word, RESET, DIM, hits[i].dist, RESET);

    free(hits);
}

/* 查一个词并打印结果；命中的规范拼写记入历史。返回 0 表示查到 */
static int lookup_and_show(dict_t *d, const char *word, strlist_t *hist)
{
    entry_t *e = dict_lookup(d, word);
    if (e) {
        show_entry(e);
        if (hist)
            strlist_push_unique(hist, e->word);
        return 0;
    }
    suggest(d, word);
    return -1;
}

static void print_help(void)
{
    printf("%s可用命令：%s\n", BOLD, RESET);
    printf("  %s<单词>%s             直接查词（大小写不敏感）\n", GREEN, RESET);
    printf("  %s:prefix%s <前缀>     列出以某前缀开头的单词（补全）\n", GREEN, RESET);
    printf("  %s:fuzzy%s  <单词>     模糊查询，按编辑距离给出近似词\n", GREEN, RESET);
    printf("  %s:add%s    <单词>     加入生词本\n", GREEN, RESET);
    printf("  %s:del%s    <单词>     从生词本删除\n", GREEN, RESET);
    printf("  %s:fav%s               查看生词本\n", GREEN, RESET);
    printf("  %s:history%s           查看查询历史\n", GREEN, RESET);
    printf("  %s:random%s            随机来一个单词\n", GREEN, RESET);
    printf("  %s:dist%s              首字母分布统计\n", GREEN, RESET);
    printf("  %s:stats%s             哈希表 / 加载统计\n", GREEN, RESET);
    printf("  %s:bench%s   <单词>     哈希查询 vs 线性扫描 性能对比\n", GREEN, RESET);
    printf("  %s:load%s    <文件>     重新加载另一本词典\n", GREEN, RESET);
    printf("  %s:clear%s             清屏\n", GREEN, RESET);
    printf("  %s:help%s / %s:quit%s       帮助 / 退出\n", GREEN, RESET, GREEN, RESET);
}

static void show_stats(const dict_t *d)
{
    printf("%s词典统计%s\n", BOLD, RESET);
    printf("  词典文件    : %s\n", d->source);
    printf("  加载耗时    : %.2f ms\n", d->load_ms);
    if (d->merged)
        printf("  合并续行    : %zu 行\n", d->merged);
    ht_print_stats(d->ht);
}

static void show_favorites(const strlist_t *fav)
{
    if (fav->count == 0) {
        printf("%s生词本还是空的，用 :add <单词> 添加%s\n", DIM, RESET);
        return;
    }
    printf("%s生词本（%zu 个）:%s\n", BOLD, fav->count, RESET);
    for (size_t i = 0; i < fav->count; i++)
        printf("  %2zu. %s\n", i + 1, fav->items[i]);
}

static void show_history(const dict_t *d, const strlist_t *hist)
{
    if (hist->count == 0) {
        printf("%s还没有查询记录%s\n", DIM, RESET);
        return;
    }
    printf("%s查询历史（%zu 条）:%s\n", BOLD, hist->count, RESET);
    for (size_t i = 0; i < hist->count; i++) {
        entry_t *e = dict_lookup((dict_t *)d, hist->items[i]);
        if (e)
            printf("  %s%s%s  %s%s%s\n", GREEN, e->word, RESET,
                   DIM, e->meaning, RESET);
    }
}

static void print_prefix(dict_t *d, const char *prefix)
{
    strlist_t out;
    strlist_init(&out);

    size_t total = dict_prefix(d, prefix, &out, PREFIX_LIMIT);

    if (total == 0) {
        printf("%s没有以 \"%s\" 开头的单词%s\n", YELLOW, prefix, RESET);
        strlist_free(&out);
        return;
    }

    printf("%s以 \"%s\" 开头共 %zu 个%s", BOLD, prefix, total, RESET);
    if (total > out.count)
        printf("%s（显示前 %zu 个）%s", DIM, out.count, RESET);
    printf(":\n");

    for (size_t i = 0; i < out.count; i++)
        printf("  %s\n", out.items[i]);

    strlist_free(&out);
}

/* 解析 "命令 参数"：把命令与参数拆开，arg 指向参数（可能是空串）*/
static void split_cmd(char *line, char **cmd, char **arg)
{
    char *sp = line;
    while (*sp && *sp != ' ')
        sp++;

    if (*sp) {
        *sp = '\0';
        *arg = util_trim(sp + 1);
    } else {
        *arg = sp;   /* 指向行尾 '\0'，即空参数 */
    }
    *cmd = line;
}

/* 处理一条以 ':' 或 '.' 开头的命令，返回 1 表示退出 */
static int handle_command(dict_t **pd, char *line, strlist_t *fav, strlist_t *hist)
{
    char *cmd, *arg;
    split_cmd(line, &cmd, &arg);

    /* 去掉前导的 ':' 或 '.' */
    if (*cmd == ':' || *cmd == '.')
        cmd++;

    if (!strcmp(cmd, "quit") || !strcmp(cmd, "q") || !strcmp(cmd, "exit"))
        return 1;

    if (!strcmp(cmd, "help") || !strcmp(cmd, "h") || !strcmp(cmd, "?")) {
        print_help();
    } else if (!strcmp(cmd, "stats")) {
        show_stats(*pd);
    } else if (!strcmp(cmd, "prefix") || !strcmp(cmd, "p")) {
        if (*arg)
            print_prefix(*pd, arg);
        else
            printf("%s用法: :prefix <前缀>%s\n", YELLOW, RESET);
    } else if (!strcmp(cmd, "fuzzy") || !strcmp(cmd, "f")) {
        if (!*arg) {
            printf("%s用法: :fuzzy <单词>%s\n", YELLOW, RESET);
        } else {
            fuzzy_hit_t *hits = NULL;
            size_t n = dict_fuzzy(*pd, arg, 3, &hits, FUZZY_LIMIT);
            if (n == 0)
                printf("%s没有找到与 \"%s\" 相近的单词%s\n", YELLOW, arg, RESET);
            for (size_t i = 0; i < n; i++)
                printf("  %s%s%s  %s(距离 %d)\n  %s%s%s\n",
                       GREEN, hits[i].entry->word, RESET, DIM, hits[i].dist,
                       RESET, hits[i].entry->meaning, RESET);
            free(hits);
        }
    } else if (!strcmp(cmd, "add")) {
        if (!*arg) {
            printf("%s用法: :add <单词>%s\n", YELLOW, RESET);
        } else {
            entry_t *e = dict_lookup(*pd, arg);
            const char *w = e ? e->word : arg;
            if (strlist_contains(fav, w)) {
                printf("%s\"%s\" 已在生词本里%s\n", DIM, w, RESET);
            } else {
                strlist_push(fav, w);
                printf("%s已加入生词本: %s%s\n", GREEN, w, RESET);
            }
        }
    } else if (!strcmp(cmd, "del")) {
        if (strlist_remove(fav, arg))
            printf("%s已从生词本删除: %s%s\n", GREEN, arg, RESET);
        else
            printf("%s生词本里没有 \"%s\"%s\n", YELLOW, arg, RESET);
    } else if (!strcmp(cmd, "fav")) {
        show_favorites(fav);
    } else if (!strcmp(cmd, "history") || !strcmp(cmd, "hist")) {
        show_history(*pd, hist);
    } else if (!strcmp(cmd, "random") || !strcmp(cmd, "r")) {
        entry_t *e = dict_random(*pd);
        if (e) {
            show_entry(e);
            strlist_push_unique(hist, e->word);
        }
    } else if (!strcmp(cmd, "dist")) {
        dict_letter_dist(*pd);
    } else if (!strcmp(cmd, "bench")) {
        if (*arg)
            dict_benchmark(*pd, arg);
        else
            printf("%s用法: :bench <单词>%s\n", YELLOW, RESET);
    } else if (!strcmp(cmd, "load")) {
        if (!*arg) {
            printf("%s用法: :load <词典文件>%s\n", YELLOW, RESET);
        } else {
            dict_t *nd = dict_load(arg);
            if (nd) {
                dict_free(*pd);
                *pd = nd;
                printf("%s已加载 %s，共 %zu 词条%s\n",
                       GREEN, nd->source, nd->ht->count, RESET);
            }
        }
    } else if (!strcmp(cmd, "clear")) {
        printf("\033[2J\033[H");
    } else {
        printf("%s未知命令: %s（输入 :help 查看帮助）%s\n", YELLOW, line, RESET);
    }

    return 0;
}

void cli_run(dict_t **pd, strlist_t *fav, strlist_t *hist)
{
    colors_init();

    char line[1024];

    printf("\n%s%sHashDict%s —— 基于哈希表的命令行英语词典\n", BOLD, CYAN, RESET);
    printf("%s  词典: %s   词条: %zu   加载: %.1f ms%s\n",
           DIM, (*pd)->source, (*pd)->ht->count, (*pd)->load_ms, RESET);
    printf("%s  输入 :help 查看命令，:quit 退出%s\n\n", DIM, RESET);

    for (;;) {
        printf("%sdict>%s ", MAGENTA, RESET);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {   /* Ctrl-D 退出 */
            printf("\n");
            break;
        }

        char *p = util_trim(line);
        if (*p == '\0')
            continue;

        if (*p == ':' || *p == '.') {
            if (handle_command(pd, p, fav, hist))
                break;
        } else {
            lookup_and_show(*pd, p, hist);
        }
    }

    printf("%s再见！%s\n", DIM, RESET);
}

int cli_lookup_once(dict_t *d, const char *word, strlist_t *hist)
{
    colors_init();
    return lookup_and_show(d, word, hist);
}

void cli_prefix_once(dict_t *d, const char *prefix, size_t limit)
{
    colors_init();

    strlist_t out;
    strlist_init(&out);

    size_t total = dict_prefix(d, prefix, &out, limit);
    if (total == 0) {
        printf("没有以 \"%s\" 开头的单词\n", prefix);
        strlist_free(&out);
        return;
    }

    printf("以 \"%s\" 开头共 %zu 个", prefix, total);
    if (total > out.count)
        printf("（显示前 %zu 个）", out.count);
    printf(":\n");

    for (size_t i = 0; i < out.count; i++)
        printf("  %s\n", out.items[i]);

    strlist_free(&out);
}
