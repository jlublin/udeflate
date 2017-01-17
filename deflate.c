#include "deflate.h"
#include "deflate_config.h"

/***** Fixed coding *****/

static uint16_t read_fixed_code()
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

static uint16_t read_fixed_distance()
{
	uint16_t code = read_huffman_bits(5);

	if(code < 4)
		return code + 1;

	uint16_t base = 1+2*(1<<((code-2)/2)) + (code&1)*(1<<((code-2)/2));

	return base + read_bits((code-2)/2);
}

static int read_fixed_block()
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

static uint8_t code_order[19] =
{
	16, 17, 18, 0, 8, 7, 9, 6, 10,
	5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static int build_code_tree(uint8_t code_lens[], uint16_t huffman_tree[], int n_codes, int n_bits)
{
	/* [symbol] => <code,bits> */
	/* TODO: implement binary tree as alternative */

	uint16_t next = 0;

	for(int i = 1; i < n_bits; i++)
	{
		for(int j = 0; j < n_codes; j++)
		{
			if(code_lens[j] == i)
			{
				huffman_tree[2*j] = next;
				huffman_tree[2*j + 1] = i;
				next += 1;
			}
		}
		next <<= 1;
	}
}

static int read_code_tree(uint16_t code_tree[], int n_codes, int n_bits)
{
	/* Read 16 bits without discarding them from input stream */
	uint16_t code = peek_huffman_bits(n_bits);

	/* Check if code matches code_tree[2*j] */
	for(int j = 0; j < n_codes; j++)
	{
		int i = code_tree[2*j+1];

		if((code >> (n_bits-i)) == code_tree[2*j] && i > 0)
		{
			/* We found the code */
			read_huffman_bits(i); /* Discard i bits from input stream */
			return j;
		}
	}

	/* Error, code was not in tree */

	return -1;
}

static int read_litlen_code(uint16_t code_tree[], uint8_t *len)
{
	int code = read_code_tree(code_tree, 19, 7);

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

static int read_huffman_tree_lens(uint16_t cl_tree[], uint8_t code_lens[], int tree_len)
{
	for(int i = 0; i < tree_len;)
	{
		uint8_t len;
		int code_len = read_litlen_code(cl_tree, &len);

		if(code_len < 0)
			return -1;

		else if(code_len < 16)
			code_lens[i++] = code_len;

		else if(code_len == 16)
			for(int j = 0; j < len; j++, i++)
				code_lens[i] = code_lens[i-1];

		else
			for(int j = 0; j < len; j++, i++)
				code_lens[i] = 0;
	}
}

static int read_dynamic_block()
{
	uint16_t hlit = read_bits(5) + 257;
	uint8_t hdist = read_bits(5) + 1;
	uint8_t hclen = read_bits(4) + 4;

	uint16_t cl_tree[19*2] = {0}; /* CL tree */
	uint16_t litlen_tree[286*2] = {0}; /* litlen tree */
	uint16_t dist_tree[32*2] = {0}; /* dist tree */

	LOG_DEBUG("(%d, %d, %d)\n", hlit, hdist, hclen);

	/* Setup CL tree */
	{
		uint8_t cl_lens[19] = {0};

		/* Read CL lengths */
		for(int i = 0; i < hclen; i++)
		{
			uint8_t code = read_bits(3);
			cl_lens[code_order[i]] = code;
		}

		for(int i = 0; i < 19; i++)
			LOG_DEBUG("Code(%d): %d\n", i, cl_lens[i]);

		/* Build CL tree */

		build_code_tree(cl_lens, cl_tree, 19, 8);

		for(int i = 0; i < 32; i++)
			LOG_DEBUG("(%d,%d)\n", cl_tree[2*i], cl_tree[2*i + 1]);
	}

	/* Setup litlen and dist trees */
	{
		uint8_t litlen_lens[286] = {0};
		uint8_t dist_lens[32] = {0};

		/* Read literal and dist tree lengths */

		read_huffman_tree_lens(cl_tree, litlen_lens, hlit);
		read_huffman_tree_lens(cl_tree, dist_lens, hdist);

		/* Debug code */

		for(int i = 0; i < hlit; i++)
			if(litlen_lens[i] != 0)
				LOG_DEBUG("! litlen %d %d\n", i, litlen_lens[i]);

		for(int i = 0; i < hdist; i++)
			if(dist_lens[i] != 0)
				LOG_DEBUG("! dist %d %d\n", i, dist_lens[i]);

		/* Build litlen and dist trees */

		build_code_tree(litlen_lens, litlen_tree, 286, 16);
		build_code_tree(dist_lens, dist_tree, 32, 16);

		/* Debug code */

		for(int i = 0; i < 286; i++)
			LOG_DEBUG("(%d,%d)\n", litlen_tree[2*i], litlen_tree[2*i + 1]);

		for(int i = 0; i < 32; i++)
			LOG_DEBUG("(%d,%d)\n", dist_tree[2*i], dist_tree[2*i + 1]);
	}

	/* Read data */

	for(int i = 0; i < 256; i++)
	{
		uint16_t code = read_code_tree(litlen_tree, 286, 16);

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
			uint16_t distance = read_code_tree(dist_tree, 32, 16);
			LOG_DEBUG("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else if(code < 285)
		{
			uint16_t len = 3 + 4*(1<<((code-261)/4)) + ((code-1)&3)*(1<<((code-261)/4));
			len += read_bits((code - 261) / 4);
			LOG_DEBUG("Code: %d (%d)\n", code, len);

			uint16_t distance = read_code_tree(dist_tree, 32, 16);
			LOG_DEBUG("Distance: %d\n", distance);

			write_match(len, distance);
		}
		else if(code == 285)
		{
			uint16_t len = 258;
			LOG_DEBUG("Code: 285 (258)\n");

			uint16_t distance = read_code_tree(dist_tree, 32, 16);
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

int deflate()
{
	while(1)
	{
		/* Read DEFLATE header */
		int last_block = read_bits(1);

		if(last_block)
			LOG_DEBUG("Last block\n");
		else
			LOG_DEBUG("Not last block\n");

		int btype = read_bits(2);

		if(btype == 0)
		{
			LOG_DEBUG("Non-compressed\n"); /* TODO */
			return -1;
		}
		else if(btype == 1)
		{
			LOG_DEBUG("Fixed Huffman\n");
			if(read_fixed_block() < 0)
				return -1;
		}
		else if(btype == 2)
		{
			LOG_DEBUG("Dynamic Huffman\n");
			if(read_dynamic_block() < 0)
				return -1;
		}
		else
		{
			LOG_DEBUG("Invalid block type 11\n");
			return -1;
		}

		if(last_block)
			break;
	}

	return 0;
}
