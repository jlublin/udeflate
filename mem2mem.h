#pragma once

#include <stdint.h>

int deflate_mem2mem_init(uint8_t *src, int src_length, uint8_t *dst, int dst_length);
int deflate_mem2mem_output_length();
