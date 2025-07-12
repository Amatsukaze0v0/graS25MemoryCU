# SystemC 安装目录（WSL 下的路径）
SYSTEMC_HOME ?= $(HOME)/systemc-3.0.1-install

CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -I$(SYSTEMC_HOME)/include -I./src -Wall -O2
CFLAGS   := -I$(SYSTEMC_HOME)/include -I./src -Wall -O2
LDFLAGS  := -L$(SYSTEMC_HOME)/lib -lsystemc

TARGET := main

# 自动收集 src 目录下所有 .cpp 和 .c 文件
CPP_SRCS := $(wildcard src/*.cpp)
C_SRCS   := $(wildcard src/*.c)
OBJS     := $(CPP_SRCS:.cpp=.o) $(C_SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 生成最终可执行文件（链接 C 和 C++ 对象）
$(TARGET): $(OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)

# 分别编译 .cpp 文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 分别编译 .c 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
.PHONY: clean
clean:
	rm -f src/*.o $(TARGET)

.PHONY: all
