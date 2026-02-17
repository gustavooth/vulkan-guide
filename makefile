APP = app
CC = clang

BIN_DIR = bin
BUILD_DIR = build
SRC_DIR = src
SHADER_DIR = shaders

SRC = $(shell find $(SRC_DIR) -name '*.c')
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

FRAG_SHADER = $(shell find $(SHADER_DIR) -name '*.frag')
VERT_SHADER = $(shell find $(SHADER_DIR) -name '*.vert')
SPV = $(patsubst %.frag, %.frag.spv, $(FRAG_SHADER)) $(patsubst %.vert, %.vert.spv, $(VERT_SHADER))

C_FLAGS = -g -fPIC -MD -Wvarargs -Wall -Werror -Wno-missing-braces -Werror=vla
INC_FLAGS = -I$(SRC_DIR) -I/usr/include
LINK_FLAGS = -lwayland-client -lvulkan -lm
DEFINES = -DPLATFORM_WAYLAND

all: $(BIN_DIR)/$(APP)

$(BIN_DIR)/$(APP): $(OBJ) $(SPV)
	$(info $@)
	mkdir -p $(BIN_DIR)
	$(CC) $(C_FLAGS) $(INC_FLAGS) $(LINK_FLAGS) $(OBJ) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(info $@)
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) $(INC_FLAGS) $(DEFINES) -c $< -o $@

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%
	glslc $< -o $@

run: $(BIN_DIR)/$(APP)
	./$(BIN_DIR)/$(APP)

clean:
	echo $(SPV)
	rm -Rf $(BUILD_DIR)