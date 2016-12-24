#include <stdio.h>
#include <stdint.h>

#if 1
#define LOG_DEBUG(x, ...) printf(x, ##__VA_ARGS__)
#define LOG_INFO(x, ...) printf(x, ##__VA_ARGS__)
#define LOG_WARNING(x, ...) printf(x, ##__VA_ARGS__)
#define LOG_ERROR(x, ...) printf(x, ##__VA_ARGS__)
#else
#define LOG_DEBUG(x, ...) do {} while(0)
#define LOG_INFO(x, ...) do {} while(0)
#define LOG_WARNING(x, ...) do {} while(0)
#define LOG_ERROR(x, ...) do {} while(0)
#endif


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
			LOG_DEBUG("Code: %d\n", code);
			write_byte(code);
		}
		else if(code == 256)
		{
			LOG_DEBUG("EOB\n");
			return 0;
		}
		else if(code < 265)
		{
			uint16_t len = code - 254;
			LOG_DEBUG("Code: %d (%d)\n", code, len);
			uint16_t distance = read_fixed_distance();
			LOG_DEBUG("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else if(code < 285)
		{
			uint16_t len = 3 + 4*(1<<((code-261)/4)) + ((code-1)&3)*(1<<((code-261)/4));
			len += read_bits((code - 261) / 4);
			LOG_DEBUG("Code: %d (%d)\n", code, len);

			uint16_t distance = read_fixed_distance();
			LOG_DEBUG("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else if(code == 285)
		{
			uint16_t len = 258;
			LOG_DEBUG("Code: 285 (258)\n");

			uint16_t distance = read_fixed_distance();
			LOG_DEBUG("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else
		{
			LOG_ERROR("Code error: %d\n", code);
			return -1;
		}
	}
}

/***** Dynamic coding *****/

uint8_t code_order[19] =
{
	16, 17, 18, 0, 8, 7, 9, 6, 10,
	5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

int build_code_len_tree(uint8_t codes[19], uint8_t code_tree[32*2])
{
	/* [value] => <code,bits> */

	uint8_t next = 0;

	for(int i = 1; i < 8; i++)
	{
		for(int j = 0; j < 19; j++)
		{
			if(codes[j] == i)
			{
				code_tree[2*next] = j;
				code_tree[2*next + 1] = i;
				next += 1;
			}
		}
		next <<= 1;
	}
}

int build_code_tree(uint8_t codes[], uint16_t code_tree[], int len)
{
	/* [code] => <value,bits> */
	/* TODO: could we use a linked list? Use 0 also and diff pointer can be 8 bits */

	uint16_t next = 0;

	for(int i = 1; i < 16; i++)
	{
		for(int j = 0; j < len; j++)
		{
			if(codes[j] == i)
			{
				code_tree[2*j] = next;
				code_tree[2*j + 1] = i;
				next += 1;
			}
		}
		next <<= 1;
	}
}

int read_code_len_tree(uint8_t code_tree[32*2])
{
	uint8_t len = code_tree[1]; /* Shortest bit length */
	uint16_t code = read_huffman_bits(len);

	/* TODO: check for max bit length? otherwise max length is 16 bits */

	for(int i = len; i < 16; i++)
	{
		/* Check if code j matches code */
		for(int j = 0; j < 32; j++)
		{
			if(j == code && i == code_tree[2*j+1])
				/* We found the code */
				return code_tree[2*j];
		}

		code = (code << 1) | read_huffman_bits(1);
	}

	/* Error, code was not in tree */

	return -1;
}

int read_code_tree(uint16_t code_tree[], int n_codes, int n_bits)
{
	uint16_t code = read_huffman_bits(1);

	for(int i = 1; i < n_bits; i++)
	{
		/* Check if code code_tree[2*j] matches code */
		for(int j = 0; j < n_codes; j++)
			if(code == code_tree[2*j] &&
			      i == code_tree[2*j+1])
				/* We found the code */
				return j;

		code = (code << 1) | read_huffman_bits(1);
	}

	/* Error, code was not in tree */

	return -1;
}

int read_litlen_code(uint8_t code_tree[], uint8_t *len)
{
	int code = read_code_len_tree(code_tree);

	if(code < 0)
	{
		LOG_ERROR("Decode error\n");
		return -1;
	}
	else if(code < 16)
		LOG_DEBUG("%d\n", code);
	else if(code == 16)
	{
		*len = read_bits(2) + 3;
		LOG_DEBUG("%d(%d)\n", code, *len);
	}
	else if(code == 17)
	{
		*len = read_bits(3) + 3;
		LOG_DEBUG("%d(%d)\n", code, *len);
	}
	else
	{
		*len = read_bits(7) + 11;
		LOG_DEBUG("%d(%d)\n", code, *len);
	}

	return code;
}

int read_dynamic_block()
{
	uint16_t hlit = read_bits(5) + 257;
	uint8_t hdist = read_bits(5) + 1;
	uint8_t hclen = read_bits(4) + 4;

	/* Read init alphabet lengths */
	uint8_t codes[19] = {0};
	uint8_t code_tree[32*2] = {0}; /* [value] => <code,bits>, code 0: invalid */
	/* TODO: we could remove half of the tree if every code was prefixed with 1 */

	LOG_DEBUG("(%d, %d, %d)\n", hlit, hdist, hclen);

	for(int i = 0; i < hclen; i++)
	{
		uint8_t code = read_bits(3);
		codes[code_order[i]] = code;
	}

	for(int i = 0; i < 19; i++)
		LOG_DEBUG("Code(%d): %d\n", i, codes[i]);

	/* Build init alphabet tree */

	build_code_len_tree(codes, code_tree);

	for(int i = 0; i < 32; i++)
		LOG_DEBUG("(%d,%d)\n", code_tree[2*i], code_tree[2*i + 1]);

	/* Read literal/length (litlen) alphabet lengths */

	uint8_t litlen_codes[286] = {0};
	uint16_t litlen_code_tree[286*2] = {0};
	/*
	 * Can't be same as before due to length....
	 * [code] => <value,bits> must be used instead
	 */

	for(int i = 0; i < hlit;)
	{
		uint8_t len;
		int litlen = read_litlen_code(code_tree, &len);

		if(litlen < 0)
			return -1;

		else if(litlen < 16)
			litlen_codes[i++] = litlen;

		else if(litlen == 16)
			for(int j = 0; j < len; j++)
				litlen_codes[i++] = litlen_codes[i-1];

		else
			for(int j = 0; j < len; j++)
				litlen_codes[i++] = 0;
	}

	/* Read distance alphabet lengths */

	uint8_t dist_codes[32] = {0};
	uint16_t dist_code_tree[32*2] = {0};

	for(int i = 0; i < hdist;)
	{
		uint8_t len;
		int dist = read_litlen_code(code_tree, &len);

		if(dist < 0)
			return -1;

		else if(dist < 16)
			dist_codes[i++] = dist;

		else if(dist == 16)
			for(int j = 0; j < len; j++, i++)
				dist_codes[i] = dist_codes[i-1];

		else
			for(int j = 0; j < len; j++)
				dist_codes[i++] = 0;
	}

	for(int i = 0; i < hlit; i++)
		if(litlen_codes[i] != 0)
			LOG_DEBUG("! litlen %d %d\n", i, litlen_codes[i]);

	for(int i = 0; i < hdist; i++)
		if(dist_codes[i] != 0)
			LOG_DEBUG("! dist %d %d\n", i, dist_codes[i]);

	/* Build litlen and dist alphabet trees */

	build_code_tree(litlen_codes, litlen_code_tree, 286);
	build_code_tree(dist_codes, dist_code_tree, 32);

	for(int i = 0; i < 286; i++)
		LOG_DEBUG("(%d,%d)\n", litlen_code_tree[2*i], litlen_code_tree[2*i + 1]);

	for(int i = 0; i < 32; i++)
		LOG_DEBUG("(%d,%d)\n", dist_code_tree[2*i], dist_code_tree[2*i + 1]);

	/* Read data */

	for(int i = 0; i < 256; i++)
	{
		uint16_t code = read_code_tree(litlen_code_tree, 286, 16);

		if(code < 256)
		{
			LOG_DEBUG("Code: %d\n", code);
			write_byte(code);
		}
		else if(code == 256)
		{
			LOG_DEBUG("EOB\n");
			return 0;
		}
		else if(code < 265)
		{
			uint16_t len = code - 254;
			LOG_DEBUG("Code: %d (%d)\n", code, len);
			uint16_t distance = read_code_tree(dist_code_tree, 32, 16);
			LOG_DEBUG("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else if(code < 285)
		{
			uint16_t len = 3 + 4*(1<<((code-261)/4)) + ((code-1)&3)*(1<<((code-261)/4));
			len += read_bits((code - 261) / 4);
			LOG_DEBUG("Code: %d (%d)\n", code, len);

			uint16_t distance = read_code_tree(dist_code_tree, 32, 16);
			LOG_DEBUG("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else if(code == 285)
		{
			uint16_t len = 258;
			LOG_DEBUG("Code: 285 (258)\n");

			uint16_t distance = read_code_tree(dist_code_tree, 32, 16);
			LOG_DEBUG("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else
		{
			LOG_ERROR("Code error: %d\n", code);
			return -1;
		}
	}

	return 0;
}

/***** Main code *****/

int read_block()
{
	/* Read zlib header */

	uint32_t cmf, flg;

	cmf = read_bits(8);
	flg = read_bits(8);

	/* Read DEFLATE header */

	if(read_bits(1))
		LOG_DEBUG("Last block\n");
	else
		LOG_DEBUG("Not last block\n");

	int btype = read_bits(2);

	if(btype == 0)
		LOG_DEBUG("Non-compressed\n"); /* TODO */
	else if(btype == 1)
	{
		LOG_DEBUG("Fixed Huffman\n");
		read_fixed_block();
	}
	else if(btype == 2)
	{
		LOG_DEBUG("Dynamic Huffman\n");
		read_dynamic_block();
	}
	else
	{
		LOG_DEBUG("Invalid block type 11\n");
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
