#ifndef HASHDICT_FUZZY_H
#define HASHDICT_FUZZY_H

/* ------------------------------------------------------------------
 * 编辑距离（Levenshtein Distance）—— 拼写纠错的基础
 * 把一个字符串改成另一个所需的"增/删/改"最少次数。
 * 例如 abandon 与 abandom 距离为 1，用来实现 "did you mean ..."
 * ------------------------------------------------------------------ */

/* 计算编辑距离（大小写不敏感）。
 * max_dist 是剪枝上限：一旦发现距离已超过它就直接返回 max_dist+1，
 * 避免为明显不相似的单词做完整 DP，这是模糊查询能跑得动的关键。
 * 返回值 > max_dist 表示"超出上限" */
int fuzzy_distance_ci(const char *a, const char *b, int max_dist);

#endif /* HASHDICT_FUZZY_H */
