#ifndef HASHDICT_CLI_H
#define HASHDICT_CLI_H

#include "dict.h"
#include "store.h"

/* ------------------------------------------------------------------
 * cli —— 命令行交互层
 * 一句话：把用户敲的东西翻译成对 dict 的调用，再把结果染个色打出来。
 * ------------------------------------------------------------------ */

/* 进入交互模式（REPL）。pd 是二级指针，因为 :load 命令会替换整本词典。
 * fav / hist 分别是生词本和历史，会被就地修改，退出后由调用方保存 */
void cli_run(dict_t **pd, strlist_t *fav, strlist_t *hist);

/* 一次性查询（命令行直接带单词时用）。返回 0 表示查到，-1 表示没查到 */
int cli_lookup_once(dict_t *d, const char *word, strlist_t *hist);

/* 一次性前缀查询（-p 选项用）*/
void cli_prefix_once(dict_t *d, const char *prefix, size_t limit);

#endif /* HASHDICT_CLI_H */
