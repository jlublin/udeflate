#include <stdio.h>
#include <stdint.h>

#include "deflate.h"

/***** I/O code *****/

/* TODO: check overruns on read/write */

uint8_t input[1 << 10]; /* 1 kiB */
uint8_t output[1 << 16]; /* 64 kiB */

int i_in; /* Next input bit index */
int i_out; /* Next output byte index */

uint32_t read_bits(int n_bits)
{
	/* Return order: x[0] x[1] etc... */

	uint32_t ret = 0;

	for(int i = 0; i < n_bits; i++, i_in++)
	{
		int next = (input[i_in >> 3] >> (i_in & 0x7)) & 1;
		ret |= next << i;
	}

	return ret;
}

uint32_t read_huffman_bits(int n_bits)
{
	/* Return order: x[n] x[n-1] etc... */

	uint32_t ret = 0;

	for(int i = 0; i < n_bits; i++, i_in++)
	{
		int next = (input[i_in >> 3] >> (i_in & 0x7)) & 1;
		ret = (ret << 1) | next;
	}

	return ret;
}

uint32_t peek_huffman_bits(int n_bits)
{
	/* Return order: x[n] x[n-1] etc... */

	uint32_t ret = 0;

	int tmp_i_in = i_in;

	for(int i = 0; i < n_bits; i++, tmp_i_in++)
	{
		int next = (input[tmp_i_in >> 3] >> (tmp_i_in & 0x7)) & 1;
		ret = (ret << 1) | next;
	}

	return ret;
}

int write_byte(uint8_t data)
{
	output[i_out++] = data;

	return 0;
}

int write_match(uint16_t len, uint16_t dist)
{
	uint8_t *ptr = &output[i_out - dist];

	for(int i = 0; i < len; i++)
		write_byte(*(ptr++));

	return 0;
}

/***** Main code *****/

int main(int argc, char **argv)
{
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

	fread(input, 1, 1024, file);

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

	deflate();

	printf("Data: ");
	for(int i = 0; i < i_out; i++)
		printf("%02x ", output[i]);
	printf("\n");

	return 0;
}
