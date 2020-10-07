#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int deflate_mem2mem_init(uint8_t *src, int src_length, uint8_t *dst, int dst_length);
int deflate_mem2mem_output_length();

#ifdef __cplusplus
};
#endif
