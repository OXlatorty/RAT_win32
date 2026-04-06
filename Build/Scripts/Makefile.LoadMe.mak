WORKSPACE := ../..
OBJ := ../obj
SRC := ../../Source

ASSEMBLY := LoadMe
EXTENSION := .exe
COMPILER_FLAGS := -g -Wall -Werror -DUNICODE -D_UNICODE
INCLUDE_FLAGS := 
LINKER_FLAGS := -g -luser32 -ladvapi32 -Wl,-subsystem:windows -Xlinker /entry:WinMainCRTStartup -Xlinker /machine:x64
DEFINES := -DUNICODE -D_UNICODE

DIR := $(subst /,\,$(WORKSPACE))
BUILD_DIR := ../bin
OBJ_DIR := $(OBJ)/$(ASSEMBLY)

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

SCR_FILES := $(call rwildcard,$(SRC)/$(ASSEMBLY)/, *.cpp)
C_FILES := $(filter %.cpp, $(SCR_FILES))
DIRECTORIES := $(sort $(dir $(C_FILES)))
SRC_PREFIX := $(SRC)/$(ASSEMBLY)/
OBJ_FILES := $(patsubst $(SRC_PREFIX)%.cpp, $(OBJ_DIR)/%.o, $(C_FILES))

all: scaffold compile link

.PHONY: scaffold
scaffold:
	@echo Scaffolding folder structure...
	-@for %%D in ($(subst $(SRC_PREFIX),,$(DIRECTORIES))) do @(setlocal enableextensions enabledelayedexpansion && mkdir "$(OBJ_DIR)/%%D" 2>NUL || cd.)
	-@setlocal enableextensions enabledelayedexpansion && mkdir "$(BUILD_DIR)" 2>NUL || cd.
	@echo Done.

.PHONY: link
link: scaffold $(OBJ_FILES)
	@echo Linking $(ASSEMBLY)...
	@clang $(COMPILER_FLAGS) $(OBJ_FILES) -o $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)

.PHONY: compile
compile:
	@echo Compiling...

.PHONY: clean
clean:
	if exist $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) del $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION)
	rmdir /s /q $(OBJ_DIR)\$(ASSEMBLY)

.PHONY: debug
debug:
	@echo ============= DEBUG INFO =============
	@echo WORKSPACE: $(WORKSPACE)
	@echo BUILD_DIR: $(BUILD_DIR)
	@echo OBJ_DIR: $(OBJ_DIR)
	@echo SCR_FILES: $(SCR_FILES)
	@echo C_FILES: $(C_FILES)
	@echo OBJ_FILES: $(OBJ_FILES)
	@echo DIRECTORIES: $(DIRECTORIES)
	@echo ====================================

$(OBJ_DIR)/%.o: $(SRC_PREFIX)%.cpp
	@echo $<...
	@mkdir "$(dir $@)" 2>NUL || cd.
	@clang $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)