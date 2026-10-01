#include "cli.h"
#include "dict.h"
#include "store.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DICT "data/dict.txt"
#define FAV_FILE     "favorites.txt"
#define HIST_FILE    "history.txt"
#define HIST_MAX     200          /* 历史最多保留多少条 */

/* 历史太长就丢掉最旧的，只留最近 HIST_MAX 条 */
static void hist_trim(strlist_t *h)
{
    if (h->count <= HIST_MAX)
        return;
    size_t drop = h->count - HIST_MAX;
    for (size_t i = 0; i < drop; i++)
        free(h->items[i]);
    memmove(h->items, h->items + drop, HIST_MAX * sizeof(char *));
    h->count = HIST_MAX;
}

static void usage(const char *prog)
{
    printf("用法: %s [选项] [单词]\n", prog);
    printf("\n");
    printf("选项:\n");
    printf("  -f <文件>     指定词典文件（默认 %s）\n", DEFAULT_DICT);
    printf("  -p <前缀>     前缀查询后退出（列出以该前缀开头的单词）\n");
    printf("  -b <单词>     对该词做哈希/线性性能基准测试后退出\n");
    printf("  -h            显示本帮助\n");
    printf("\n");
    printf("不带选项时进入交互模式；带一个单词则查询后退出。\n");
    printf("示例:\n");
    printf("  %s abandon              # 查一个词\n", prog);
    printf("  %s -p aband             # 列出以 aband 开头的词\n", prog);
    printf("  %s -b abandon           # 性能基准\n", prog);
    printf("  %s                      # 进入交互模式\n", prog);
}

/* 把生词本/历史的存储路径拼到 ~/.hashdict 下 */
static void build_store_path(char *buf, size_t n, const char *file)
{
    snprintf(buf, n, "%s/%s", store_dir(), file);
}

int main(int argc, char **argv)
{
    const char *dictpath  = DEFAULT_DICT;
    const char *prefix    = NULL;
    const char *benchword = NULL;
    const char *oneword   = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            dictpath = argv[++i];
        } else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            prefix = argv[++i];
        } else if (!strcmp(argv[i], "-b") && i + 1 < argc) {
            benchword = argv[++i];
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "未知选项: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        } else {
            oneword = argv[i];
        }
    }

    /* 1) 加载词典到哈希表 */
    dict_t *d = dict_load(dictpath);
    if (!d)
        return 1;

    /* 2) 加载生词本与历史（文件不存在也没关系）*/
    char favpath[1024], histpath[1024];
    build_store_path(favpath, sizeof(favpath), FAV_FILE);
    build_store_path(histpath, sizeof(histpath), HIST_FILE);

    strlist_t fav, hist;
    strlist_init(&fav);
    strlist_init(&hist);
    strlist_load(&fav, favpath);
    strlist_load(&hist, histpath);
    hist_trim(&hist);

    /* 3) 按模式分派 */
    int rc = 0;

    if (benchword) {
        dict_benchmark(d, benchword);
    } else if (prefix) {
        cli_prefix_once(d, prefix, 40);
    } else if (oneword) {
        rc = cli_lookup_once(d, oneword, &hist);
    } else {
        cli_run(&d, &fav, &hist);   /* 交互模式，d 可能被 :load 替换 */
    }

    /* 4) 存回生词本与历史 */
    hist_trim(&hist);
    strlist_save(&fav, favpath);
    strlist_save(&hist, histpath);

    strlist_free(&fav);
    strlist_free(&hist);
    dict_free(d);

    return rc == 0 ? 0 : 1;
}
