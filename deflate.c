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

#include "deflate.h"
#include "deflate_config.h"

/*
 * Error codes:
 *
 * EINVAL    - Is returned when input data is invalid
 * x         - Any code returned from I/O code
 */

/***** Read functions *****/

static uint32_t in_bits;
static int n_in = 0; /* Current number of bits in in_bits */
static int i_in = 0; /* Next input bit index */

static int fetch_bits(int bits_needed)
{
	int ret;

	if(bits_needed > 24)
		return -EINVAL;

	/* Remove any whole bytes we've consumed */
	int remove_bits = i_in & 0xf8;
	i_in &= 0x7;
	n_in -= remove_bits;
	in_bits >>= remove_bits;

	/* Read up to bytes needed */
	int n = (bits_needed - (n_in - i_in) + 7) / 8;
	for(int i = 0; i < n; i++)
	{
		if((ret = deflate_read_byte()) < 0)
			return ret;


		/* Put them last as most significant bits */
		in_bits |= ret << n_in;
		n_in += 8;
	}

	return 0;
}

static int read_next_byte()
{
	int ret;

	/* Align i_in to byte boundary */
	int offset = (i_in + 7) & 0x7;
	in_bits >>= offset;
	i_in &= 0x7;
	n_in -= 8;

	if((ret = fetch_bits(8)) < 0)
		return ret;

	ret = (in_bits >> i_in) & 0xff;
	i_in += 8;

	return ret;
}

static int read_bits(int n_bits)
{
	/* Return order: x[0] x[1] etc... */

	int ret;
	int bits = 0;

	if((ret = fetch_bits(n_bits)) < 0)
		return ret;

	for(int i = 0; i < n_bits; i++, i_in++)
	{
		int next = (in_bits >> i_in) & 1;
		bits |= next << i;
	}

	return bits;
}

static int read_huffman_bits(int n_bits)
{
	/* Return order: x[n] x[n-1] etc... */

	int ret;
	int bits = 0;

	if((ret = fetch_bits(n_bits)) < 0)
		return ret;

	for(int i = 0; i < n_bits; i++, i_in++)
	{
		int next = (in_bits >> i_in) & 1;
		bits = (bits << 1) | next;
	}

	return bits;
}

static int peek_huffman_bits(int n_bits)
{
	/* Return order: x[n] x[n-1] etc... */

	int ret;
	int bits = 0;

	if((ret = fetch_bits(n_bits)) < 0)
		return ret;

	int peek_i_in = i_in;

	for(int i = 0; i < n_bits; i++, peek_i_in++)
	{
		int next = (in_bits >> peek_i_in) & 1;
		bits = (bits << 1) | next;
	}

	return bits;
}

/***** General functions *****/
static const int EOB = 0x10000;
static int decode_symbol(uint16_t code)
{
	int ret = 0;

	if(code < 256)
	{
		LOG_DEBUG("Code: %d\n", code);
		deflate_write_byte(code);
	}

	else if(code == 256)
	{
		LOG_DEBUG("EOB\n");
		ret = EOB;
	}
	else if(code < 265)
	{
		uint16_t len = code - 254;
		LOG_DEBUG("Code: %d (%d)\n", code, len);

		ret = len;
	}
	else if(code < 285)
	{
		uint16_t len = 3 + 4*(1<<((code-261)/4)) + ((code-1)&3)*(1<<((code-261)/4));
		len += read_bits((code - 261) / 4);
		LOG_DEBUG("Code: %d (%d)\n", code, len);

		ret = len;
	}
	else if(code == 285)
	{
		uint16_t len = 258;
		LOG_DEBUG("Code: 285 (258)\n");

		ret = len;
	}
	else
	{
		LOG_ERROR("Code error: %d\n", code);
		ret = -EINVAL;
	}

	return ret;
}

/***** Non-compressed block *****/

static int read_non_compressed_block()
{
	uint8_t len = read_next_byte();
	uint8_t nlen = read_next_byte();

	if(len != ~nlen)
		return -EINVAL;

	return deflate_write_input_bytes(len);
}

/***** Fixed Huffman coding block *****/

static int read_fixed_code()
{
	int ret;
	uint16_t code;

	if((ret = read_huffman_bits(7)) < 0)
		return ret;

	code = ret;

	if(code < 0x18)
		return code + 256;

	if((ret = read_huffman_bits(1)) < 0)
		return ret;

	code <<= 1;
	code |= ret;

	if(code < 0xc0)
		return code - 0x30;

	if(code < 0xc8)
		return code + 0x58;

	if((ret = read_huffman_bits(1)) < 0)
		return ret;

	code <<= 1;
	code |= ret;

	return code - 0x100;
}

static int read_fixed_distance()
{
	int ret;
	uint16_t code;

	if((ret = read_huffman_bits(5)) < 0)
		return ret;

	code = ret;

	if(code < 4)
		return code + 1;

	uint16_t base = 1+2*(1<<((code-2)/2)) + (code&1)*(1<<((code-2)/2));

	return base + read_bits((code-2)/2);
}

static int read_fixed_block()
{
	while(1)
	{
		int ret;
		uint16_t code;
		uint16_t len;

		if((ret = read_fixed_code()) < 0)
			return ret;

		code = ret;

		if((ret = decode_symbol(code)) < 0)
			return ret;

		if(ret < 0)
			return len;

		if(ret == EOB)
			break;

		len = ret;

		if(len != 0)
		{
			if((ret = read_fixed_distance()) < 0)
				return ret;

			uint16_t distance = ret;

			LOG_DEBUG("Match: %d, %d\n", len, distance);
			if((ret = deflate_write_match(len, distance)) < 0)
				return ret;
		}
	}

	return 0;
}

/***** Dynamic Huffman coding block *****/

static uint8_t code_order[19] =
{
	16, 17, 18, 0, 8, 7, 9, 6, 10,
	5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static void build_code_tree(uint8_t code_lens[], uint16_t huffman_tree[], int n_codes, int n_bits)
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
	int ret;
	uint16_t code;

	/* Read n bits without discarding them from input stream */
	if((ret = peek_huffman_bits(n_bits)) < 0)
		return ret;

	code = ret;

	/* Check if code matches code_tree[2*j] */
	for(int j = 0; j < n_codes; j++)
	{
		int i = code_tree[2*j+1];

		if((code >> (n_bits-i)) == code_tree[2*j] && i > 0)
		{
			/* We found the code */
			/* Discard the i bits from input stream */
			if((ret = read_huffman_bits(i)) < 0)
				return ret;

			return j;
		}
	}

	/* Error, code was not in tree */
	return -EINVAL;
}

static int read_litlen_code(uint16_t code_tree[], uint8_t *len)
{
	int ret;
	uint16_t code;

	if((ret = read_code_tree(code_tree, 19, 7)) < 0)
	{
		LOG_ERROR("Decode error\n");
		return ret;
	}

	code = ret;

	if(code < 16)
		LOG_DEBUG("%d\n", code);
	else if(code == 16)
	{
		if((ret = read_bits(2)) < 0)
			return ret;

		*len = ret + 3;
		LOG_DEBUG("%d(%d)\n", code, *len);
	}
	else if(code == 17)
	{
		if((ret = read_bits(3)) < 0)
			return ret;

		*len = ret + 3;
		LOG_DEBUG("%d(%d)\n", code, *len);
	}
	else
	{
		if((ret = read_bits(7)) < 0)
			return ret;

		*len = ret + 11;
		LOG_DEBUG("%d(%d)\n", code, *len);
	}

	return code;
}

static int read_dynamic_distance(uint16_t dist_tree[])
{
	int ret;
	uint16_t code;

	if((ret = read_code_tree(dist_tree, 32, 16)) < 0)
		return ret;

	code = ret;

	if(code < 4)
		return code + 1;

	uint16_t base = 1+2*(1<<((code-2)/2)) + (code&1)*(1<<((code-2)/2));

	return base + read_bits((code-2)/2);
}

static int read_huffman_tree_lens(uint16_t cl_tree[], uint8_t code_lens[], int tree_len)
{
	for(int i = 0; i < tree_len;)
	{
		int ret;
		uint8_t len;
		uint16_t code_len;

		if((ret = read_litlen_code(cl_tree, &len)) < 0)
			return ret;

		code_len = ret;

		if(code_len < 16)
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
	int ret;
	uint16_t hlit;
	uint8_t hdist;
	uint8_t hclen;

	if((ret = read_bits(5)) < 0)
		return ret;

	hlit = ret + 257;

	if((ret = read_bits(5)) < 0)
		return ret;

	hdist = ret + 1;

	if((ret = read_bits(4)) < 0)
		return ret;

	hclen = ret + 4;

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
			if((ret = read_bits(3)) < 0)
				return ret;

			uint8_t code = ret;
			cl_lens[code_order[i]] = code;
		}

		for(int i = 0; i < 19; i++)
			LOG_DEBUG("Code(%d): %d\n", i, cl_lens[i]);

		/* Build CL tree */

		build_code_tree(cl_lens, cl_tree, 19, 8);

		for(int i = 0; i < 19; i++)
			LOG_DEBUG("(%d,%d)\n", cl_tree[2*i], cl_tree[2*i + 1]);
	}

	/* Setup litlen and dist trees */
	{
		uint8_t litlen_lens[286] = {0};
		uint8_t dist_lens[32] = {0};

		/* Read literal and dist tree lengths */

		if((ret = read_huffman_tree_lens(cl_tree, litlen_lens, hlit)) < 0)
			return ret;
		if((ret = read_huffman_tree_lens(cl_tree, dist_lens, hdist)) < 0)
			return ret;

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

		LOG_DEBUG("Litlen tree:\n");
		for(int i = 0; i < 286; i++)
			LOG_DEBUG("%d: (%d,%d)\n", i, litlen_tree[2*i], litlen_tree[2*i + 1]);

		LOG_DEBUG("Dist tree:\n");
		for(int i = 0; i < 32; i++)
			LOG_DEBUG("%d: (%d,%d)\n", i, dist_tree[2*i], dist_tree[2*i + 1]);
	}

	/* Read data */

	while(1)
	{
		if((ret = read_code_tree(litlen_tree, 286, 16)) < 0)
			return ret;

		uint16_t code = ret;

		if((ret = decode_symbol(code)) < 0)
			return ret;

		if(ret == EOB)
			break;

		uint16_t len = ret;

		if(len != 0)
		{
			if((ret = read_dynamic_distance(dist_tree)) < 0)
				return ret;

			uint16_t distance = ret;

			LOG_DEBUG("Match: %d, %d\n", len, distance);
			if((ret = deflate_write_match(len, distance)) < 0)
				return ret;
		}
	}

	return 0;
}

/***** Main code *****/

int deflate()
{
	while(1)
	{
		int ret;

		/* Read DEFLATE header */
		if((ret = read_bits(1)) < 0)
			return ret;

		int last_block = ret;

		if(last_block)
			LOG_DEBUG("Last block\n");
		else
			LOG_DEBUG("Not last block\n");

		if((ret = read_bits(2)) < 0)
			return ret;

		int btype = ret;

		if(btype == 0)
		{
			LOG_DEBUG("Non-compressed\n");
			if((ret = read_non_compressed_block()) < 0)
				return ret;
		}
		else if(btype == 1)
		{
			LOG_DEBUG("Fixed Huffman\n");
			if((ret = read_fixed_block()) < 0)
				return ret;
		}
		else if(btype == 2)
		{
			LOG_DEBUG("Dynamic Huffman\n");
			if((ret = read_dynamic_block()) < 0)
				return ret;
		}
		else
		{
			LOG_DEBUG("Invalid block type 11\n");
			return -EINVAL;
		}

		if(last_block)
			break;
	}

	return 0;
}
