APP = app
CC = clang

BIN_DIR = bin
BUILD_DIR = build
SRC_DIR = src

SRC = $(shell find $(SRC_DIR) -name '*.c')
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

C_FLAGS = -g -fPIC -MD -Wvarargs -Wall -Werror -Wno-missing-braces -Werror=vla
INC_FLAGS = -I$(SRC_DIR) -I/usr/include
LINK_FLAGS = -lwayland-client -lvulkan -lm

all: $(BIN_DIR)/$(APP)

$(BIN_DIR)/$(APP): $(OBJ)
	$(info $@)
	mkdir -p $(BIN_DIR)
	$(CC) $(C_FLAGS) $(INC_FLAGS) $(LINK_FLAGS) $(OBJ) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(info $@)
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) $(INC_FLAGS) -c $< -o $@

run: $(BIN_DIR)/$(APP)
	./$(BIN_DIR)/$(APP)