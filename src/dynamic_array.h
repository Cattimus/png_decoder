#pragma once

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

//only holds arrays of uint8_t(bytes)
typedef struct Dynamic_array
{
	//raw byte pointer to data
	uint8_t* data;

	//information about the number of objects stored in array
	uint64_t count;
	uint64_t max_size;

	//position in the bit stream (for PNG decoding)
	uint64_t byte_position;
	uint64_t bit_position;

}	dynamic_array;

//generic array functions
dynamic_array* create_array();
void free_array(dynamic_array* to_free);
void array_add(dynamic_array* arr, void* data, uint64_t count);
void push_byte(dynamic_array* arr, uint8_t data);
uint8_t array_get(dynamic_array* arr, uint64_t index);

//helper functions for bit streams
char pull_bit(dynamic_array* to_pull);
uint32_t pull_bits(dynamic_array* to_pull, uint8_t length);
void next_boundry(dynamic_array* cur);