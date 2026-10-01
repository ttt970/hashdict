# ============================================================
#  HashDict —— 基于哈希表的命令行英语词典
#  常用目标：
#    make            编译（生成 hashdict）
#    make run        编译并进入交互模式
#    make debug      带 ASan/UBSan 的调试版（抓内存越界和泄漏）
#    make memcheck   valgrind 内存检查
#    make clean      清理
# ============================================================

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS :=

SRCDIR  := src
OBJDIR  := build
BIN     := hashdict
DICT    := data/dict.txt

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all run debug memcheck clean rebuild

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# -MMD -MP 自动生成头文件依赖，改了 .h 也会重新编译对应的 .o
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

# 开发时直接编译并进入交互模式
run: all
	./$(BIN) -f $(DICT)

# 调试版：地址消毒器 + 未定义行为消毒器，能抓越界/泄漏/悬空指针
debug: CFLAGS += -g -O0 -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: rebuild

# valgrind 内存检查（需先安装 valgrind）
memcheck: all
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./$(BIN) -f $(DICT) abandon

rebuild: clean all

clean:
	rm -rf $(OBJDIR) $(BIN)

-include $(DEPS)
