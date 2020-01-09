CC = gcc
LD = gcc
BINARY = deflate
BUILD = build
SHELL = /bin/bash

OBJ = main.o deflate.o

TARGET_BINARY = $(addprefix $(BUILD)/, $(BINARY))
TARGET_OBJ = $(addprefix $(BUILD)/, $(OBJ))

TEST_VECTOR = dot gradient white
TARGET_TEST_VECTOR = $(addprefix tests/, $(addsuffix .deflate, $(TEST_VECTOR)))

all: $(TARGET_BINARY)

clean:
	rm -f $(TARGET_BINARY)
	rm -f $(TARGET_OBJ)
	rmdir $(BUILD)

test:
	echo $(TARGET_OBJ)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET_BINARY): $(TARGET_OBJ)
	$(LD) -o $(TARGET_BINARY) $(TARGET_OBJ)

$(TARGET_OBJ): $(BUILD)/%.o: %.c
	mkdir -p $(BUILD)
	$(CC) -c -o $@ $<

main.c: deflate.h
deflate.c: deflate.h deflate_config.h

check: $(TARGET_BINARY) $(TARGET_TEST_VECTOR)

%.deflate: %.out
	@echo
	@echo Testing $@
	@echo '######################################################'
	@diff <($(TARGET_BINARY) $@ | grep 'Data:') $< && echo 'Success!' || (echo 'Failure!' ; exit 1)
