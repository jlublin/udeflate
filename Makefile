CC = gcc
LD = gcc
BINARY = deflate
BUILD = build
SHELL = /bin/bash

OBJ = main.o deflate.o

TARGET_BINARY = $(addprefix $(BUILD)/, $(BINARY))
TARGET_OBJ = $(addprefix $(BUILD)/, $(OBJ))

PYTHON_TESTS = $(wildcard tests/test*.py)
SH_TESTS = $(wildcard tests/test*.sh)
C_TESTS = $(wildcard tests/test*.c)
TESTS = $(PYTHON_TESTS) $(SH_TESTS) $(C_TESTS)

all: $(TARGET_BINARY)

clean:
	rm -f $(TARGET_BINARY)
	rm -f $(TARGET_OBJ)
	rmdir $(BUILD)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET_BINARY): $(TARGET_OBJ)
	$(LD) -o $(TARGET_BINARY) $(TARGET_OBJ)

$(TARGET_OBJ): $(BUILD)/%.o: %.c
	mkdir -p $(BUILD)
	$(CC) -c -o $@ $<

main.c: deflate.h
deflate.c: deflate.h deflate_config.h

check: $(TARGET_BINARY) $(TESTS)

$(TESTS):
	@printf "Testing $@: "
	@TARGET=$(PWD)/$(TARGET_BINARY) \
	BUILD_DIR=$(PWD)/$(BUILD) \
	TESTS_DIR=$(PWD)/tests \
	$@ \
	&& echo 'Success!' || (echo 'Failure!'; exit 1)

.PHONY: clean $(TESTS)
