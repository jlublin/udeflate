#pragma once

#include <stdint.h>

/***** Application defined functions *****/
uint32_t read_bits(int n_bits);
uint32_t read_huffman_bits(int n_bits);
int write_byte(uint8_t data);
int write_match(uint16_t len, uint16_t dist);

/***** Deflate functions *****/
int read_block();
