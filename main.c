/******************************************************************************

MIT License

Copyright (c) 2017 Joachim Lublin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "deflate.h"

/***** I/O code *****/

/*
 * Error codes:
 *
 * EOVERFLOW - Is returned when output buffer is to small
 * EINVAL    - Is returned when input data is invalid
 *             (i.e. read/write outside of buffers)
 */

#define IN_SIZE (1 << 10) /* 1 kiB */
#define OUT_SIZE (1 << 16) /* 64 kiB */

uint8_t input[IN_SIZE];
uint8_t output[OUT_SIZE];

int i_in; /* Next input byte index */
int i_out; /* Next output byte index */

int read_byte()
{
	/* Check input is OK with another aligned byte */
	if(i_in + 1 > IN_SIZE)
		return -EINVAL;

	return input[i_in++];
}

int write_byte(uint8_t data)
{
	/* Check output is OK with another byte */
	if((i_out + 1) > OUT_SIZE)
		return -ENOMEM;

	output[i_out++] = data;

	return 0;
}

int write_match(uint16_t len, uint16_t dist)
{
	/* Check output is OK with len */
	if((i_out + len) > OUT_SIZE)
		return -ENOMEM;

	/* Check output is OK with dist */
	if(i_out - dist < 0)
		return -EINVAL;

	/* Byte by byte copy, memcpy doesn't work on overlapping regions */
	uint8_t *ptr = &output[i_out - dist];

	for(int i = 0; i < len; i++)
		output[i_out++] = *(ptr++);

	return 0;
}

int write_input_bytes(uint16_t len)
{
	/* Check input and output is OK with len */
	if((i_out + len) > OUT_SIZE)
		return -ENOMEM;

	if(i_in + len > IN_SIZE)
		return -EINVAL;

	memcpy(&output[i_out], &input[i_in], len);

	i_out += len;
	i_in += len;

	return 0;
}

/***** Main code *****/

int main(int argc, char **argv)
{
	int ret;

	if(argc < 2)
	{
		printf("Usage: %s <file>\n", argv[0]);
		return -1;
	}

	FILE *file;

	if((file = fopen(argv[1], "rb")) == NULL)
	{
		printf("Unable to open file %s\n", argv[1]);
		return -1;
	}

	ret = fread(input, 1, 1024, file);

	if(ferror(file))
	{
		printf("Unable to read data from file\n");
		return -1;
	}

	if(feof(file) == 0)
	{
		printf("Unable to read all data from file\n");
		return -1;
	}

	i_in = 0;
	i_out = 0;

	if((ret = deflate()) < 0)
	{
		printf("Error running DEFLATE (%d)\n", ret);
		return -1;
	}

	printf("Data: ");
	for(int i = 0; i < i_out; i++)
		printf("%02x ", output[i]);
	printf("\n");

	return 0;
}
