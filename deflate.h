#pragma once

#include <stdint.h>
#include <errno.h>

/***** Application defined functions *****/
int deflate_read_byte();
int deflate_write_byte(uint8_t data);
int deflate_write_match(uint16_t len, uint16_t dist);
int deflate_write_input_bytes(uint16_t len);

/***** Deflate functions *****/
int deflate();
