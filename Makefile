default: all
#################
# Configuration #
#################

# Source files
SRC = main.c deflate.c mem2mem.c

# Header dependencies
main.c: deflate.h mem2mem.h
deflate.c: deflate.h deflate_config.h

#################
# Default tools #
#################

CC = gcc
LD = gcc
BINARY = deflate
BUILD = build
SHELL = /bin/bash

#######################
# Generated variables #
#######################

OBJ = $(addprefix $(BUILD)/, $(SRC:.c=.o))
TARGET_BINARY = $(addprefix $(BUILD)/, $(BINARY))

PYTHON_TESTS = $(wildcard tests/test*.py)
SH_TESTS = $(wildcard tests/test*.sh)
C_TESTS = $(wildcard tests/test*.c)
TESTS = $(sort $(PYTHON_TESTS) $(SH_TESTS) $(C_TESTS))

#########
# Rules #
#########

all: $(TARGET_BINARY)

clean:
	rm -f $(TARGET_BINARY)
	rm -f $(OBJ)
	rmdir $(BUILD)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET_BINARY): $(OBJ)
	$(LD) -o $(TARGET_BINARY) $(OBJ)

$(OBJ): $(BUILD)/%.o: %.c $(BUILD)
	$(CC) -c -o $@ $<

check: $(TARGET_BINARY) $(TESTS)

$(TESTS):
	@printf "Testing $@: "
	@TARGET=$(PWD)/$(TARGET_BINARY) \
	BUILD_DIR=$(PWD)/$(BUILD) \
	TESTS_DIR=$(PWD)/tests \
	$@ \
	&& echo 'Success!' || (echo 'Failure!'; exit 1)

.PHONY: clean $(TESTS)
