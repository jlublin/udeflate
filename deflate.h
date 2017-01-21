#pragma once

#include <stdint.h>
#include <errno.h>

/***** Application defined functions *****/
int read_bits(int n_bits);
int read_huffman_bits(int n_bits);
int peek_huffman_bits(int n_bits); /* TODO combine with read_huffman_bits? */
int read_next_byte();
int write_byte(uint8_t data);
int write_match(uint16_t len, uint16_t dist);
int write_input_bytes(uint16_t len);

/***** Deflate functions *****/
int deflate();
