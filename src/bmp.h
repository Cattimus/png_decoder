#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Bmp
{
	uint32_t w;
	uint32_t h;
	uint32_t pixel_width;

	uint8_t* pixel_data;

	int is_valid;
}bmp;

void write_bmp(const void* pixel_data, int width, int height, const char* filename);
bmp* read_bmp(const char* filename);
void free_bmp(bmp* to_free);