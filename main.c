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
#include <errno.h>

#include "deflate.h"
#include "mem2mem.h"

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

	if((ret = deflate_mem2mem_init(input, IN_SIZE, output, OUT_SIZE)) < 0)
	{
		printf("Invalid arguments to deflate_mem2mem_init\n");
		return -1;
	}

	if((ret = deflate()) < 0)
	{
		printf("Error running DEFLATE (%d)\n", ret);
		return -1;
	}

	printf("Data: ");
	for(int i = 0; i < deflate_mem2mem_output_length(); i++)
		printf("%02x ", output[i]);
	printf("\n");

	return 0;
}
