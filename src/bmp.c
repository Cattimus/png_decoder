#include "bmp.h"

void free_bmp(bmp* to_free)
{
	if(to_free != NULL)
	{
		if(to_free->pixel_data != NULL)
		{
			free(to_free->pixel_data);
			to_free->pixel_data = NULL;
		}

		free(to_free);
		to_free = NULL;
	}
}

//write bmp file given a pixel array. only supports RGB pixel format.
//mostly meant as a validation that the file format readers work properly
void write_bmp(const void* pixel_data, int width, int height, const char* filename)
{
	//construct bmp file in memory first
	uint32_t filesize = 0;

	uint32_t row_size = 3 * width;
	uint32_t padding_size = (row_size % 4);

	uint32_t data = 0;

	//allocate far more data than we need
	uint8_t* output = calloc(width * height * 3 * 2, 1);

	output[0] = 'B';
	output[1] = 'M';
	filesize += 2;

	//these 4 bytes need to contain the size of the bmp file in bytes
	filesize += 4;

	//skip 2 reserved data segments
	filesize += 4;

	//offset pixel array can be found
	data = 56;
	memcpy(output+filesize, &data, 4);
	filesize += 4;

	//size of header #2 (40 bytes)
	data = 40;
	memcpy(output+filesize, &data, 4);
	filesize += 4;

	//bitmap width in pixels
	memcpy(output+filesize, &width, 4);
	filesize += 4;

	//bitmap height in pixels
	memcpy(output+filesize, &height, 4);
	filesize += 4;

	//number of color panes (always 1)
	data = 1;
	memcpy(output+filesize, &data, 2);
	filesize += 2;

	//number of bits per pixel
	int bits_per_pixel = 3 * 8;
	memcpy(output+filesize, &bits_per_pixel, 2);
	filesize += 2;

	//compression method being used
	filesize += 4;

	//image size
	filesize += 4;

	//horizontal resolution
	filesize += 4;

	//vertical resolution
	filesize += 4;

	//number of colors in color palette
	filesize += 4;

	//always ignored but sill present for some reason
	filesize += 4;
	filesize += 2;

	for(int y = height - 1; y >= 0; y--)
	{
		int row_index = 0;
		for(int x = 0; x < width; x++)
		{
			const uint8_t* data_index = (uint8_t*)pixel_data + (y * row_size) + x * 3;
			uint8_t* output_index = output + filesize;

			//write RGB backwards because this is the worst file format known to man
			output_index[0] = data_index[2];
			output_index[1] = data_index[1];
			output_index[2] = data_index[0];

			filesize += 3;
		}

		//add padding if necessary(if it's not it'll just be 0)
		filesize += padding_size;
	}

	//update filesize parameter
	memcpy(output+2, &filesize, 4);

	//write png to disk
	FILE* out = fopen(filename, "wb");
	fwrite(output, 1, filesize, out);
	fclose(out);

	free(output);
}

bmp* read_bmp(const char* filename)
{
	bmp* to_return = calloc(1, sizeof(bmp));

	FILE* input = fopen(filename, "r");
	if(input == NULL)
	{
		fprintf(stderr, "read_bmp: Failed to open file %s. BMP creation aborted.\n", filename);
		return to_return;
	}

	char header[3];
	if(fread(&header, 2, 1, input) != 1)
	{
		fclose(input);
		fprintf(stderr, "read_bmp: Failed to read '%s'. BMP creation aborted\n", filename);
		return to_return;
	}
	header[2] = 0;

	int is_bmp = strncmp("BM", header, 2);
	if(is_bmp != 0)
	{
		printf("File is invalid bmp image\n");
		exit(1);
	}

	int filesize = 0;
	if(fread(&filesize, 4, 1, input) != 0)
	{
		fclose(input);
		fprintf(stderr, "read_bmp: Failed to read '%s'. BMP creation aborted\n", filename);
		return to_return;
	}
	fseek(input, 0, SEEK_SET);
	
	//read entire file into memory
	uint8_t* bmp_data = malloc(filesize);
	if(fread(bmp_data, filesize, 1, input) != 1)
	{
		fclose(input);
		fprintf(stderr, "read_bmp: Failed to read '%s'. BMP creation aborted\n", filename);
		return to_return;
	}
	fclose(input);

	int pixel_offset = (int)*(bmp_data + 10);
	memcpy(&to_return->w, bmp_data+18, 4);
	memcpy(&to_return->h, bmp_data+22, 4);
	
	int pixel_temp = 0;
	memcpy(&pixel_temp, bmp_data+28, 2);
	to_return->pixel_width = pixel_temp / 8;

	if(to_return->pixel_width != 3)
	{
		fprintf(stderr, "read_bmp: unsupported pixel format: %d. BMP creation aborted.\n", to_return->pixel_width);
		return to_return;
	}

	int compression_method = (int)*(bmp_data+30);

	if(compression_method != 0)
	{
		fprintf(stderr, "unsupported compression method: %d. BMP creation aborted.\n", compression_method);
		return to_return;
	}

	//array our pixels are going to be stored in. RGB format
	to_return->pixel_data = calloc(to_return->w * to_return->h * to_return->pixel_width, 1);

	int index = 0;
	int row_size = to_return->pixel_width * to_return->w;
	int padding_size = (row_size % 4);

	for(int y = to_return->h - 1; y >= 0; y--)
	{
		for(int x = 0; x < to_return->w; x++)
		{
			uint8_t* output_index = to_return->pixel_data + (y * row_size) + x * to_return->pixel_width;
			uint8_t* data_index = bmp_data + pixel_offset + index;

			//read RGB backwards because this is the worst file format known to man
			output_index[0] = data_index[2];
			output_index[1] = data_index[1];
			output_index[2] = data_index[0];

			index += 3;
		}

		index += padding_size;
	}

	free(bmp_data);

	to_return->is_valid = 1;
	return to_return;
}