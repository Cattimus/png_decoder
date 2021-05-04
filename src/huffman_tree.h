#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "dynamic_array.h"

//only need a basic binary tree data structure for huffman coding
typedef struct Node node;
typedef struct Node
{
	uint32_t symbol;
	node* left;
	node* right;

	int is_leaf;
}node;

//assemble huffman tree
node* create_node();
void add_node(node* root, uint32_t symbol, uint32_t code, uint32_t code_length);
void free_tree(node* root);

//retrieve functions
int32_t get_symbol(dynamic_array* cur, node* root);
void traverse(node** cur, char bit);

//tree generation helper functions
node* static_symbol();
node* static_distance();
node* create_dynamic_tree(uint32_t* code_lengths, uint32_t num_codes);
node *create_alphabet(uint32_t *code_lengths, uint32_t num_codes);