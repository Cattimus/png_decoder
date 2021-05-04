#include "png.h"

//PNG file signature to compare against
static const char png_signature[9] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

//hand-coded lookup tables for symbols 257-285 (lengths)
static uint32_t length_values[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static uint32_t length_extra_bits[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

//hand-coded lookup tables for (length,distance) pairs
static uint32_t distance_values[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static uint32_t distance_extra_bits[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

//helper functions for file reading
static int handle_chunk(png *png, int chunk_length, const char *chunk_header, FILE *png_file);
static void handle_IDAT(png *png, int length, FILE *png_file);
static int handle_IHDR(png *png, int length, FILE *png_file);
static int is_required(char input);

//decode encoded png data to pixel data
static void decode_png(png *cur);
static int handle_zlib(png *cur);

//helper functions for different compressed data block types
static void uncompressed_block(dynamic_array *cur, dynamic_array *output_stream);
static void huffman_block(dynamic_array *cur, dynamic_array *output_stream, node *literal_tree, node *distance_tree);
static void handle_length_copy(dynamic_array *cur, dynamic_array *output_stream, int32_t length, int32_t distance);

//helper function for dynamic huffman trees
static void generate_dynamic(dynamic_array *cur, node **literal_tree, node **distance_tree);
static node *decode_dynamic_tree(node *alphabet, dynamic_array *cur, uint32_t num_codes);

//helper functions for reversing filter on decoded pixels
static void handle_filter(png *cur, dynamic_array *output_stream);
static int32_t paeth(int32_t a, int32_t b, int32_t c);

//print all the relevant info about an (already read) png
void png_info(png *to_print)
{
	if (!to_print->is_valid)
	{
		printf("PNG file supplied either has not been initialized or is invalid\n");
		return;
	}

	printf("width: %d\n", to_print->w);
	printf("height: %d\n", to_print->h);
	printf("color type: %d\n", to_print->color_type);
	printf("bit depth: %d\n", to_print->bytes_per_pixel);
	printf("filter method: %d\n", to_print->filter_method);
	printf("interlace method: %d\n", to_print->interlace_method);
}

//free all dynamic memory from the png (while avoiding double free())
void free_png(png *to_free)
{
	if (to_free != NULL)
	{
		if (to_free->raw_data != NULL)
		{
			free_array(to_free->raw_data);
			to_free->raw_data = NULL;
		}

		if (to_free->pixel_data != NULL)
		{
			free_array(to_free->pixel_data);
			to_free->pixel_data = NULL;
		}

		free(to_free);
		to_free = NULL;
	}
}

//read and decode png from file name
png *read_png(const char *filename)
{
	png *to_return = calloc(1, sizeof(png));

	to_return->is_valid = 0;
	to_return->pixel_data = create_array();
	to_return->raw_data = create_array();

	FILE *png_file = fopen(filename, "rb");
	if (png_file == NULL)
	{
		fprintf(stderr, "read_png: Failed to open file %s. PNG creation aborted.\n", filename);
		return to_return;
	}

	//check that file header matches PNG
	char file_header[8];
	if (fread(file_header, 1, 8, png_file) != 8)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return to_return;
	}
	if (strncmp(file_header, png_signature, 8) != 0)
	{
		fclose(png_file);
		fprintf(stderr, "read_png: input file does not match PNG header. PNG creation aborted\n");
		return to_return;
	}

	//loop through chunks
	while (!to_return->is_valid)
	{
		int chunk_length = 0;
		char chunk_type[5];
		chunk_type[4] = '\0';

		if (fread(&chunk_length, 4, 1, png_file) != 1)
		{
			fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
			return to_return;
		}

		if (fread(&chunk_type, 4, 1, png_file) != 1)
		{
			fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
			return to_return;
		}
		//fix endian-ness (default stored in big endian)
		if(check_endian())
		{
			chunk_length = byte_swap(chunk_length);
		}

		if (is_required(chunk_type[0]))
		{
			if (!handle_chunk(to_return, chunk_length, chunk_type, png_file))
			{
				fprintf(stderr, "read_png: PNG contains features that are not supported in chunk: %s\n", chunk_type);
				fclose(png_file);
				return to_return;
			}
		}

		//unecessary chunks are ignored
		else
		{
			fseek(png_file, chunk_length, SEEK_CUR);
		}

		//skip CRC bytes (CRC check is unsupported)
		fseek(png_file, 4, SEEK_CUR);
	}
	fclose(png_file);

	decode_png(to_return);

	//free unecessary data
	free_array(to_return->raw_data);
	to_return->raw_data = NULL;

	return to_return;
}

//helper function(for code clarity)
//as per the png specification, bit 5(range 0 to 7) of the first byte denotes whether a chunk is critical or ancillary
static int is_required(char input)
{
	return ((input & 0x20) == 0);
}

//copy the data to the raw_data buffer
static void handle_IDAT(png *png, int length, FILE *png_file)
{
	char *data = calloc(length, 1);
	if (fread(data, 1, length, png_file) != length)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return;
	}
	array_add(png->raw_data, data, length);
	free(data);
}

//takes all the data from IHDR chunk and moves it to png object
static int handle_IHDR(png *png, int length, FILE *png_file)
{
	if (fread(&png->w, 4, 1, png_file) != 1)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return 0;
	}

	if (fread(&png->h, 4, 1, png_file) != 1)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return 0;
	}

	if (fread(&png->bytes_per_pixel, 1, 1, png_file) != 1)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return 0;
	}

	if (fread(&png->color_type, 1, 1, png_file) != 1)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return 0;
	}

	if (fread(&png->compression_method, 1, 1, png_file) != 1)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return 0;
	}

	if (fread(&png->filter_method, 1, 1, png_file) != 1)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return 0;
	}

	if (fread(&png->interlace_method, 1, 1, png_file) != 1)
	{
		fprintf(stderr, "read_png: file read failed. Png creation aborted\n");
		return 0;
	}

	//fix byte order of the multi-byte numbers
	if(check_endian)
	{
		png->w = byte_swap(png->w);
		png->h = byte_swap(png->h);
	}

	//unsupported options
	if (png->color_type != 2 && png->color_type != 6)
	{
		return 0;
	}
	if (png->bytes_per_pixel != 8)
	{
		return 0;
	}
	if (png->interlace_method != 0)
	{
		return 0;
	}

	//additional helpful info (for my brain anyways)
	if (png->color_type == 2)
	{
		png->bytes_per_pixel = 3;
	}
	if (png->color_type == 6)
	{
		png->bytes_per_pixel = 4;
	}

	return 1;
}

//handle all necessary chunks to parse a basic png. 1 is success, 0 is failure
//only color types 2 and 6 are supported. Any file containing PLTE chunk will error out
static int handle_chunk(png *png, int chunk_length, const char *chunk_header, FILE *png_file)
{
	if (strncmp(chunk_header, "IHDR", 4) == 0)
	{
		if (!handle_IHDR(png, chunk_length, png_file))
		{
			return 0;
		}
		return 1;
	}
	if (strncmp(chunk_header, "IDAT", 4) == 0)
	{
		handle_IDAT(png, chunk_length, png_file);
		return 1;
	}
	if (strncmp(chunk_header, "IEND", 4) == 0)
	{
		png->is_valid = 1;
		return 1;
	}

	return 0;
}

//decode a png that has been read into memory
static void decode_png(png *cur)
{
	//array our output will be copied to
	dynamic_array *output_stream = create_array();

	//these trees are defined here since they will be the same for every block
	node *static_literal_tree = static_symbol();
	node *static_distance_tree = static_distance();

	//there is exactly 1 zlib header at the start of the compressed data
	if (!handle_zlib(cur))
	{
		free_array(output_stream);
		fprintf(stderr, "decode_png: compressed data stream contains flags that are not supported by PNG specification. Cannot decode data.\n");
		return;
	}

	//iterate through blocks
	char is_final = 0;
	while (!is_final)
	{
		//read block header
		is_final = pull_bit(cur->raw_data);
		char type = pull_bits(cur->raw_data, 2);

		node *dynamic_literal_tree;
		node *dynamic_distance_tree;

		switch (type)
		{
		//uncompressed
		case 0:
			uncompressed_block(cur->raw_data, output_stream);
			break;

		//fixed huffman tree
		case 1:
			huffman_block(cur->raw_data, output_stream, static_literal_tree, static_distance_tree);
			break;

		//dynamic huffman tree
		case 2:
			generate_dynamic(cur->raw_data, &dynamic_literal_tree, &dynamic_distance_tree);
			huffman_block(cur->raw_data, output_stream, dynamic_literal_tree, dynamic_distance_tree);
			free_tree(dynamic_literal_tree);
			free_tree(dynamic_distance_tree);
			break;

		//error
		case 3:
			free_array(output_stream);
			fprintf(stderr, "decode_png: compressed data stream contains a block with type 3(error). Cannot decode data.\n");
			return;
			break;
		}
	}

	//clean up dynamic memory before moving on
	free_tree(static_literal_tree);
	free_tree(static_distance_tree);

	//remove filtering from output data(converts it to pixel data)
	handle_filter(cur, output_stream);

	//free the decoded output stream
	free_array(output_stream);
}

//will read zlib header to see if any special attention is needed
static int handle_zlib(png *cur)
{
	uint8_t cmf = pull_bits(cur->raw_data, 8);
	uint8_t flg = pull_bits(cur->raw_data, 8);

	//png format only supports compression method 8 (DEFLATE)
	uint8_t compression_method = cmf & 0x0F;
	if (compression_method != 8)
	{
		return 0;
	}

	//check for dictionary to see how many bytes we need to skip
	if ((flg & 0x20) > 0)
	{
		cur->raw_data->byte_position += 4;
	}

	return 1;
}

//copy data from uncompressed block
static void uncompressed_block(dynamic_array *cur, dynamic_array *output_stream)
{
	printf("Uncompressed block has been reached\n");
	next_boundry(cur);

	uint32_t length = pull_bits(cur, 16);
	if(!check_endian())
	{
		length = (uint32_t)byte_swap(length);
	}

	cur->byte_position += 2;
	array_add(output_stream, cur->data + cur->byte_position, length);
	cur->byte_position += length + 4;
}

//helper function to decode huffman-encoded block
static void huffman_block(dynamic_array *cur, dynamic_array *output_stream, node *literal_tree, node *distance_tree)
{
	while (1)
	{
		int32_t symbol = get_symbol(cur, literal_tree);

		if (symbol < 256)
		{
			uint8_t temp = (uint8_t)symbol;
			push_byte(output_stream, temp);
		}
		if (symbol == 256)
		{
			break;
		}
		if (symbol > 256)
		{
			//calculate the length
			int32_t index = symbol - 257;
			int32_t length_bits = length_extra_bits[index];
			int32_t length = length_values[index];
			length += pull_bits(cur, length_bits);

			//retreive distance from table
			int32_t distance_code = get_symbol(cur, distance_tree);
			int32_t distance_bits = distance_extra_bits[distance_code];
			int32_t distance = distance_values[distance_code] + pull_bits(cur, distance_bits);

			handle_length_copy(cur, output_stream, length, distance);
		}
	}
}

static void handle_length_copy(dynamic_array *cur, dynamic_array *output_stream, int32_t length, int32_t distance)
{
	int32_t max = output_stream->count;
	int32_t current_position = max - distance;

	//hard crash to avoid undefined behavior if we see values that would result in memory access violations
	if (current_position < 0)
	{
		fprintf(stderr, "decode_png: corruption or error detected - distance has pointed to a location before the start of the output array. %d\n", distance);
		abort();
	}

	int32_t counter = 0;
	while (counter < length)
	{
		int8_t data_temp = array_get(output_stream, current_position);
		push_byte(output_stream, data_temp);
		counter++;
		current_position++;

		if (current_position >= max)
		{
			current_position = max - distance;
		}
	}
}

static void handle_filter(png *cur, dynamic_array *output_stream)
{
	int32_t scanline_size = cur->w * cur->bytes_per_pixel;

	//iterate through scanlines
	int32_t scanline_counter = 0;
	while (scanline_counter < cur->h)
	{
		int32_t position = (scanline_counter * scanline_size) + scanline_counter;
		int32_t output_position = (scanline_counter * scanline_size);

		uint8_t filter_method = array_get(output_stream, position);

		//iterate through scanline
		for (int i = 0; i < scanline_size; i++)
		{
			uint8_t x = 0;

			uint8_t a = 0;
			uint8_t b = 0;
			uint8_t c = 0;

			x = array_get(output_stream, (position + i) + 1);

			if ((i >= (cur->bytes_per_pixel)) && (scanline_counter > 0))
			{
				c = array_get(cur->pixel_data, (output_position + i - scanline_size - cur->bytes_per_pixel));
			}
			if (scanline_counter > 0)
			{
				b = array_get(cur->pixel_data, (output_position + i - scanline_size));
			}
			if (i >= (cur->bytes_per_pixel))
			{
				a = array_get(cur->pixel_data, (output_position + i - cur->bytes_per_pixel));
			}

			int32_t to_add = 0;
			//calculate output pixel
			switch (filter_method)
			{
			//none
			case 0:
				push_byte(cur->pixel_data, x);
				break;

			//sub
			case 1:
				to_add = (x + a);
				push_byte(cur->pixel_data, to_add);
				break;

			//up
			case 2:
				to_add = (x + b);
				push_byte(cur->pixel_data, to_add);
				break;

			//average
			case 3:
				to_add = x + floor((a + b) / 2);
				push_byte(cur->pixel_data, to_add);
				break;

			//paeth
			case 4:
				to_add = x + paeth(a, b, c);
				push_byte(cur->pixel_data, to_add);
				break;
			}
		}

		scanline_counter++;
	}
}

static int32_t paeth(int32_t a, int32_t b, int32_t c)
{
	int32_t p = a + b - c;
	int32_t pa = abs(p - a);
	int32_t pb = abs(p - b);
	int32_t pc = abs(p - c);
	if (pa <= pb && pa <= pc)
	{
		return a;
	}
	else if (pb <= pc)
	{
		return b;
	}

	return c;
}

//generate literal and static huffman trees for dynamic block
static void generate_dynamic(dynamic_array *cur, node **literal_tree, node **distance_tree)
{
	//pull info about block header from data stream
	uint32_t HLIT = (pull_bits(cur, 5) + 257);
	uint32_t HDIST = (pull_bits(cur, 5) + 1);
	uint32_t HCLEN = (pull_bits(cur, 4) + 4);

	//pull code lengths from data stream
	uint32_t *alphabet_code_lengths = calloc(19, sizeof(uint32_t));
	for (int i = 0; i < HCLEN; i++)
	{
		alphabet_code_lengths[i] = pull_bits(cur, 3);
	}

	//create tree for "alphabet", used to decode the two other trees
	//this needs it's own special function because of the funky order of the code lengths
	node *alphabet_tree = create_alphabet(alphabet_code_lengths, HCLEN);

	//decode the two dynamic trees we need from data stream (order matters)
	*literal_tree = decode_dynamic_tree(alphabet_tree, cur, HLIT);
	*distance_tree = decode_dynamic_tree(alphabet_tree, cur, HDIST);

	free_tree(alphabet_tree);
	free(alphabet_code_lengths);
}

//given an alphabet tree, decode the code lengths of the tree
static node *decode_dynamic_tree(node *alphabet, dynamic_array *cur, uint32_t num_codes)
{
	uint32_t *code_lengths = calloc(num_codes, sizeof(uint32_t));
	int length_index = 0;

	//decode loop for code lengths of literal alphabet
	uint32_t result = 0;
	uint32_t previous_code = 0;
	while (length_index != num_codes && length_index < num_codes)
	{
		result = 0;
		result = get_symbol(cur, alphabet);

		uint32_t extra_arg = 0;
		switch (result)
		{
		//copy last value 3 - 6 times (2 extra bits)
		case 16:
			extra_arg = pull_bits(cur, 2);
			for (int x = 0; x < (3 + extra_arg); x++)
			{
				code_lengths[length_index] = previous_code;
				length_index++;
			}
			break;

		//copy null 3-11 times (3 exta bits)
		case 17:
			extra_arg = pull_bits(cur, 3);
			for (int x = 0; x < (3 + extra_arg); x++)
			{
				code_lengths[length_index] = 0;
				length_index++;
			}
			break;

		//copy null 11-138 times (7 exta bits)
		case 18:
			extra_arg = pull_bits(cur, 7);
			for (int x = 0; x < (11 + extra_arg); x++)
			{
				code_lengths[length_index] = 0;
				length_index++;
			}
			break;

		//default value (copy value)
		default:
			code_lengths[length_index] = result;
			previous_code = result;
			length_index++;
			break;
		}
	}

	node *to_return = create_dynamic_tree(code_lengths, num_codes);
	free(code_lengths);

	return to_return;
}

//check if host system is little_endian or big_endian
int check_endian()
{
	int32_t num = 1;

	if(*(int8_t*)&num = 1)
	{
		return 1;
	}

	return 0;
}

//swap bytes for endianess (this function is defined for cross-compiler compatability)
int32_t byte_swap(int32_t to_swap)
{
	int32_t to_return = 0;

	to_return += ((to_swap & 0x000000FF) << 24);
	to_return += ((to_swap & 0x0000FF00) << 8);
	to_return += ((to_swap & 0x00FF0000) >> 8);
	to_return += ((to_swap & 0xFF000000) >> 24);

	return to_return;
}