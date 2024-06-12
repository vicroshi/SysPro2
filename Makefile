CC = gcc
DEPFLAGS = -MT $@ -MMD -MP
INCLUDE_DIR = ./include
CFLAGS= -g -Wall -I$(INCLUDE_DIR)
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) -c
BUILD_DIR = ./build
SOURCE_DIR = ./src
BIN_DIR = ./bin
SRCS := $(wildcard $(SOURCE_DIR)/*.c)
MODS := $(filter-out $(SOURCE_DIR)/jobCommander.c $(SOURCE_DIR)/jobExecutorServer.c, $(SRCS))
OBJS := $(patsubst $(SOURCE_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
DEPS := $(OBJS:%.o=%.d)
PATH  := $(PATH):$(BIN_DIR)
jobCommanderBIN := $(BIN_DIR)/jobCommander
jobExecutorServerBIN := $(BIN_DIR)/jobExecutorServer
EXECS = jobCommander jobExecutorServer
SRC_EXECS = $(addprefix $(SOURCE_DIR)/, $(EXECS:%=%.c))
LIST = $(addprefix $(BIN_DIR)/, $(EXECS))
.PHONY: all clean
all: $(LIST)

$(EXECS): $(BIN_DIR)/$@

$(BIN_DIR)/jobCommander: $(BUILD_DIR)/jobCommander.o
		$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/jobExecutorServer: $(filter-out $(BUILD_DIR)/jobCommander.o, $(OBJS))
		$(CC) $(CFLAGS) -pthread $^ -o $@

build/%.o : $(SOURCE_DIR)/%.c
		$(COMPILE.c) $(OUTPUT_OPTION) $<

clean:
	rm -rf $(BIN_DIR)/* $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d

-include $(DEPS)