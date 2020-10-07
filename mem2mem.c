#include "deflate.h"

#include <string.h>

static uint8_t *src;
static uint8_t *dst;

static int src_length;
static int dst_length;

static int i_src; /* Next input byte index */
static int i_dst; /* Next output byte index */

int deflate_mem2mem_init(uint8_t *_src, int _src_length, uint8_t *_dst, int _dst_length)
{
	if(!_src || ! _dst || _src_length < 1 || _dst_length < 1)
		return -EINVAL;

	src = _src;
	dst = _dst;
	src_length = _src_length;
	dst_length = _dst_length;
	i_src = 0;
	i_dst = 0;

	return 0;
}

int deflate_mem2mem_output_length()
{
	return i_dst;
}

int deflate_read_byte()
{
	/* Check input is OK with another aligned byte */
	if(i_src + 1 > src_length)
		return -EINVAL;

	return src[i_src++];
}

int deflate_write_byte(uint8_t data)
{
	/* Check output is OK with another byte */
	if((i_dst + 1) > dst_length)
		return -ENOMEM;

	dst[i_dst++] = data;

	return 0;
}

int deflate_write_match(uint16_t len, uint16_t dist)
{
	/* Check output is OK with len */
	if((i_dst + len) > dst_length)
		return -ENOMEM;

	/* Check output is OK with dist */
	if(i_dst - dist < 0)
		return -EINVAL;

	/* Byte by byte copy, memcpy doesn't work on overlapping regions */
	uint8_t *ptr = &dst[i_dst - dist];

	for(int i = 0; i < len; i++)
		dst[i_dst++] = *(ptr++);

	return 0;
}

int deflate_write_input_bytes(uint16_t len)
{
	/* Check input and output is OK with len */
	if((i_dst + len) > dst_length)
		return -ENOMEM;

	if(i_src + len > src_length)
		return -EINVAL;

	memcpy(&dst[i_dst], &src[i_src], len);

	i_dst += len;
	i_src += len;

	return 0;
}
