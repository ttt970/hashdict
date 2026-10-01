#!/usr/bin/env bash
# ============================================================
#  HashDict 功能自测
#  用法:
#     make && ./tests/run_tests.sh
#  或指定可执行文件/词典:
#     BIN=./hashdict DICT=data/dict.txt ./tests/run_tests.sh
# ============================================================
set -u

BIN="${BIN:-./hashdict}"
DICT="${DICT:-data/dict.txt}"
PASS=0
FAIL=0

# 用独立的 HOME，避免污染真实的 ~/.hashdict
export HOME="$(mktemp -d)"
export NO_COLOR=1

if [ ! -x "$BIN" ]; then
    echo "找不到可执行文件 $BIN，请先 make"
    exit 1
fi

# check <描述> <期望子串> <实际输出>
check() {
    local desc="$1" expect="$2" actual="$3"
    if printf '%s' "$actual" | grep -qF -- "$expect"; then
        printf '  \033[32mPASS\033[0m  %s\n' "$desc"
        PASS=$((PASS + 1))
    else
        printf '  \033[31mFAIL\033[0m  %s\n' "$desc"
        printf '        期望包含: %s\n' "$expect"
        printf '        实际输出: %s\n' "$actual"
        FAIL=$((FAIL + 1))
    fi
}

echo "== 精确查询 =="
out=$("$BIN" -f "$DICT" abandon)
check "查 abandon 命中" "go away from" "$out"

echo "== 大小写不敏感 =="
out=$("$BIN" -f "$DICT" ABANDON)
check "ABANDON 也能查到" "go away from" "$out"

echo "== 未命中 + 拼写纠错 =="
out=$("$BIN" -f "$DICT" abandom)
check "未命中时提示未找到" "未找到" "$out"
check "给出近似词 abandon" "abandon" "$out"

echo "== 前缀补全 =="
out=$("$BIN" -f "$DICT" -p aband)
check "前缀命中 abandon" "abandon" "$out"
check "前缀命中 abandonment" "abandonment" "$out"

echo "== CRLF 处理（释义不应带尾部 \\r）=="
out=$("$BIN" -f "$DICT" abandon | tr -d '\r')
check "释义行无残留回车符" "desert" "$out"

echo "== 折行释义合并（overburden）=="
out=$("$BIN" -f "$DICT" overburden)
check "两条折行被合并" "to load with excessive weight" "$out"

echo "== 性能基准 =="
out=$("$BIN" -f "$DICT" -b abandon)
check "基准测试输出加速比" "加速比" "$out"

echo "== 生词本持久化 =="
printf ':add abandon\n:quit\n' | "$BIN" -f "$DICT" >/dev/null 2>&1
out=$(printf ':fav\n:quit\n' | "$BIN" -f "$DICT")
check "重启后生词本仍在" "abandon" "$out"

echo "== 未知选项退出码 =="
"$BIN" -z >/dev/null 2>&1
check "未知选项返回非 0" "1" "$?"

echo
echo "----------------------------------------"
printf '通过 %d 项，失败 %d 项\n' "$PASS" "$FAIL"
rm -rf "$HOME"
[ "$FAIL" -eq 0 ]
