#ifndef HASHDICT_UTIL_H
#define HASHDICT_UTIL_H

#include <stddef.h>

/* ------------------------------------------------------------------
 * util —— 通用工具：单调时钟、字符串处理、跨平台的目录/终端判断
 * 单独抽出来是为了让 hash / dict / cli 各模块都能复用，避免重复造轮子
 * ------------------------------------------------------------------ */

/* 单调时钟，返回毫秒。Linux 下用 clock_gettime(CLOCK_MONOTONIC)，
 * 不受系统时间调整影响，适合测加载/查询耗时 */
double util_now_ms(void);

/* malloc 一份字符串副本，失败返回 NULL（不依赖 POSIX 的 strdup） */
char *util_strdup(const char *s);

/* 原地去掉首尾空白（空格/制表符/\r/\n），返回去掉头部空白后的指针。
 * 注意返回值可能不等于入参，且入参内容被修改 */
char *util_trim(char *s);

/* 大小写不敏感的字符串比较：相等返回 1，否则 0（效率等价 strcmp，
 * 不需要先 malloc+strlwr） */
int util_streq_ci(const char *a, const char *b);

/* 比较前 n 个字符，大小写不敏感：相同返回 1 */
int util_strneq_ci(const char *a, const char *b, size_t n);

/* stdout 是否连在终端上——决定要不要输出 ANSI 颜色。
 * 设置 NO_COLOR 环境变量可强制关闭 */
int util_isatty_stdout(void);

/* 递归创建目录（等价 mkdir -p），已存在也视为成功 */
int util_mkdir_p(const char *path);

/* 取用户主目录：Linux 用 HOME，Windows 用 USERPROFILE，都没有则 "." */
const char *util_home_dir(void);

#endif /* HASHDICT_UTIL_H */
