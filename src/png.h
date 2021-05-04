#pragma once

#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "dynamic_array.h"
#include "huffman_tree.h"

typedef struct Png
{
	//raw data read from file
	//this gets free'd once png has been decoded
	dynamic_array* raw_data;

	//data decoded from png
	//this is empty before png is decoded
	dynamic_array* pixel_data;

	//info provided by IHDR
	int w;
	int h;
	uint8_t bytes_per_pixel;
	uint8_t bits_per_pixel;
	uint8_t color_type;
	uint8_t compression_method;
	uint8_t filter_method;
	uint8_t interlace_method;

	//flag to show whether a PNG has been read correctly or not
	int is_valid;
}png;

png* read_png(const char* filename);
void png_info(png* to_print);
void free_png(png* to_free);

//returns 1 if little endian, 0 if big endian
int check_endian();
int32_t byte_swap(int32_t to_swap);