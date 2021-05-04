#include <stdio.h>

#include "png.h"
#include "bmp.h"

int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		fprintf(stderr, "Invalid arguments. Example usage: png_decoder [input.png] [output.bmp].\n");
	}

	png* to_convert = read_png(argv[1]);
	if(to_convert->is_valid)
	{
		write_bmp(to_convert->pixel_data->data, to_convert->w, to_convert->h, argv[2]);
	}

	free_png(to_convert);
	return 0;
}