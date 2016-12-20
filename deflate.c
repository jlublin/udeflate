#include <stdio.h>
#include <stdint.h>

/***** General code *****/

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

/***** Fixed coding *****/

uint16_t read_fixed_code()
{
	uint16_t code = read_huffman_bits(7);

	if(code < 0x18)
		return code + 256;

	code <<= 1;
	code |= read_huffman_bits(1);

	if(code < 0xc0)
		return code - 0x30;

	if(code < 0xc8)
		return code + 0x58;

	code <<= 1;
	code |= read_huffman_bits(1);

	return code - 0x100;
}

uint16_t read_fixed_distance()
{
	uint16_t code = read_huffman_bits(5);

	if(code < 4)
		return code + 1;

	uint16_t base = 1+2*(1<<((code-2)/2)) + (code&1)*(1<<((code-2)/2));

	return base + read_bits((code-2)/2);
}

int read_fixed_block()
{
	while(1)
	{
		uint16_t code = read_fixed_code();

		if(code < 256)
		{
			printf("Code: %d\n", code);
			write_byte(code);
		}
		else if(code == 256)
		{
			printf("EOB\n");
			return 0;
		}
		else if(code < 265)
		{
			uint16_t len = code - 254;
			printf("Code: %d (%d)\n", code, len);
			uint16_t distance = read_fixed_distance();
			printf("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else if(code < 285)
		{
			uint16_t len = 3 + 4*(1<<((code-261)/4)) + ((code-1)&3)*(1<<((code-261)/4));
			len += read_bits((code - 261) / 4);
			printf("Code: %d (%d)\n", code, len);

			uint16_t distance = read_fixed_distance();
			printf("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else if(code == 285)
		{
			uint16_t len = 258;
			printf("Code: 285 (258)\n");

			uint16_t distance = read_fixed_distance();
			printf("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else
		{
			printf("Code error: %d\n", code);
			return -1;
		}
	}
}

/***** Dynamic coding *****/

/***** Main code *****/

int read_block()
{
	/* Read zlib header */

	uint32_t cmf, flg;

	cmf = read_bits(8);
	flg = read_bits(8);

	/* Read DEFLATE header */

	if(read_bits(1))
		printf("Last block\n");
	else
		printf("Not last block\n");

	int btype = read_bits(2);

	if(btype == 0)
		printf("Non-compressed\n");
	else if(btype == 1)
	{
		printf("Fixed Huffman\n");
		read_fixed_block();
	}
	else if(btype == 2)
		printf("Dynamic Huffman\n");
	else
	{
		printf("Invalid block type 11\n");
		return -1;
	}
}

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

	read_block();

	printf("Data: ");
	for(int i = 0; i < i_out; i++)
		printf("%02x ", output[i]);
	printf("\n");

	return 0;
}
