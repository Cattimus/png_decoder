#include "dynamic_array.h"

static int expand_array(dynamic_array* to_expand, uint64_t count);

//initialize new array
dynamic_array* create_array()
{
	dynamic_array* to_return = calloc(1, sizeof(dynamic_array));

	to_return->data = calloc(25, 1);
	to_return->max_size = 25;

	to_return->bit_position = 0;
	to_return->byte_position = 0;
	return to_return;
}

//add items to array(auto expands when necessary)
void array_add(dynamic_array* arr, void* data, uint64_t count)
{
	uint64_t free_slots = (arr->max_size - arr->count);

	//check if array needs to be expanded
	if(count > free_slots)
	{
		//check for failure in array resize
		if(!expand_array(arr, (arr->max_size + count + 10000)))
		{
			fprintf(stderr, "dynamic_array: unable to allocate new memory. data copy of size %lu has been aborted\n", count);
			return;
		}
	}

	memcpy(arr->data + arr->count, data, count);
	arr->count += count;
}

void push_byte(dynamic_array* arr, uint8_t data)
{
	uint64_t free_slots = (arr->max_size - arr->count);

	//check if array needs to be expanded
	if(free_slots < 1)
	{
		//check for failure in array resize
		if(!expand_array(arr, (arr->max_size + 10000)))
		{
			fprintf(stderr, "dynamic_array: unable to allocate new memory. data copy of size %d has been aborted\n", 1);
			return;
		}
	}

	memcpy(arr->data + arr->count, &data, 1);
	arr->count++;
}

//attempt to resize the array. 0 is failure, 1 is success
static int expand_array(dynamic_array* to_expand, uint64_t count)
{
	uint8_t* temp = realloc(to_expand->data, count);
	if(temp == NULL)
	{
		return 0;
	}

	to_expand->data = temp;
	to_expand->max_size = count;
	return 1;
}

//free all dynamically allocated memory (avoiding double free())
void free_array(dynamic_array* to_free)
{
	if(to_free != NULL)
	{
		if(to_free->data != NULL)
		{
			free(to_free->data);
		}
		free(to_free);
	}
	
}

//return pointer to element at index
uint8_t array_get(dynamic_array* arr, uint64_t index)
{
	if(index > arr->max_size)
	{
		fprintf(stderr, "dynamic array: Unable to fetch index: %lu. Max index is: %lu. First element has been returned.\n", index, arr->max_size);
		return *(arr->data);
	}

	return *(arr->data + index);
}

//pull a single bit out of the bit stream. keeps track of position
char pull_bit(dynamic_array* to_pull)
{
	uint8_t data = array_get(to_pull, to_pull->byte_position);
	uint8_t to_and = 1;
	to_and <<= (to_pull->bit_position);

	//increment bit counter
	to_pull->bit_position++;

	//move to the next boundry once the end of the byte is reached
	if(to_pull->bit_position == 8)
	{
		next_boundry(to_pull);
	}

	return ((data & to_and) > 0);
}

//pull length number of bits from the current position in the bit stream
//original bit order is preserved (for interpretation as a number)
uint32_t pull_bits(dynamic_array* to_pull, uint8_t length)
{
	uint32_t to_return = 0;
	uint32_t to_add = 1;
	to_add <<= (length - 1);
	for(int i = 0; i < length; i++)
	{
		if(pull_bit(to_pull) > 0)
		{
			to_return += to_add;
		}

		if(i < (length - 1))
		{
			to_return >>= 1;
		}
	}

	return to_return;
}

//skip the rest of the unread bits in a byte
void next_boundry(dynamic_array* cur)
{
	cur->bit_position = 0;
	cur->byte_position++;
}