#include "huffman_tree.h"

//this lookup table is required because the alphabet code lengths are stored in a very strange manner
static uint32_t alphabet_indexes[] = {3, 17, 15, 13, 11, 9, 7, 5, 4, 6, 8, 10, 12, 14, 16, 18, 0, 1, 2};
static uint32_t reverse_bits(uint32_t input, uint32_t num_bits);
static uint32_t* generate_codes(uint32_t* code_lengths, uint32_t num_codes);

//adds a new node to the tree given a code and length
void add_node(node* root, uint32_t symbol, uint32_t code, uint32_t code_length)
{	
	node* cur = root;

	for(int i = 0; i < code_length; i++)
	{
		if((code & 0x01) == 1)
		{
			if(cur->right != NULL)
			{
				cur = cur->right;
			}
			else
			{
				cur->right = create_node();
				cur = cur->right;
			}
		}
		
		if((code & 0x01) == 0)
		{
			if(cur->left != NULL)
			{
				cur = cur->left;
			}
			else
			{
				cur->left = create_node();
				cur = cur->left;
			}
		}
		
		code >>= 1;
	}
	cur->symbol = symbol;
	cur->is_leaf = 1;
	return;
}

//traverse one level of a node (does not check if leaf)
void traverse(node** cur, char bit)
{
	if(bit)
	{
		if((*cur)->right != NULL)
		{
			*cur = (*cur)->right;
		}
		else
		{
			fprintf(stderr, "Decoding errors detected - program attempted to access a node that is set to null\n");
		}
	}
	else
	{
		if((*cur)->left != NULL)
		{
			*cur = (*cur)->left;
		}
		else
		{
			fprintf(stderr, "Decoding errors detected - program attempted to access a node that is set to null\n");
		}
	}
}

//helper function to simplify syntax
node* create_node()
{
	node* to_return = calloc(1, sizeof(node));
	to_return->left = NULL;
	to_return->right = NULL;
	to_return->is_leaf = 0;
	return to_return;
}

//recursive free (avoids double free())
void free_tree(node* root)
{
	if(root->right != NULL)
	{
		free_tree(root->right);
	}
	if(root->left != NULL)
	{
		free_tree(root->left);
	}

	if(root != NULL)
	{
		free(root);
		root = NULL;
	}
}

//generate static literal tree
node* static_symbol()
{
	node* to_return = create_node();

	uint32_t code = 0x30;
	for(uint32_t i = 0; i < 144; i++)
	{
		add_node(to_return, i, reverse_bits(code, 8), 8);
		code++;
	}

	code = 0x190;
	for(uint32_t i = 144; i < 256; i++)
	{
		add_node(to_return, i, reverse_bits(code, 9), 9);
		code++;
	}

	code = 0;
	for(uint32_t i = 256; i < 280; i++)
	{
		add_node(to_return, i, reverse_bits(code, 7), 7);
		code++;
	}

	code = 0xC0;
	for(uint32_t i = 280; i < 288; i++)
	{
		add_node(to_return, i, reverse_bits(code, 8), 8);
		code++;
	}

	return to_return;
}

//generate static distance tree
node* static_distance()
{
	node* to_return = create_node();

	for(uint32_t i = 0; i < 30; i++)
    {
        add_node(to_return, i, reverse_bits(i, 5), 5);
    }

	return to_return;
}

//helper function for huffman coding. the codes are required to be reversed to work properly
static uint32_t reverse_bits(uint32_t input, uint32_t num_bits)
{
    uint32_t to_return = 0;
    for(int i = 0; i < num_bits; i++)
    {
        to_return = to_return + (input & 0x01);

        if(i != num_bits - 1)
        {
            to_return = to_return << 1;
            input = input >> 1;
        }
    }

    return to_return;
}

//traverse tree until symbol is found
int32_t get_symbol(dynamic_array* cur, node* root)
{
	node* tree = root;

	while(!tree->is_leaf)
	{
		traverse(&tree, pull_bit(cur));
	}
	return tree->symbol;
}

//generate the huffman codes given code lengths
static uint32_t* generate_codes(uint32_t* code_lengths, uint32_t num_codes)
{
	/*count how many of each code lengths we have
	 *it is presumed that no code length can be over 32 bits in length
	 *since that would sort of defeat the point of huffman coding*/
	uint32_t code_max_bits = 32;
	uint32_t array_max = 0;
	uint32_t* bl_count = calloc((code_max_bits + 1), sizeof(uint32_t));

	for(int i = 0; i < num_codes; i++)
	{
		if(code_lengths[i] > array_max)
		{
			array_max = code_lengths[i];
		}

		bl_count[code_lengths[i]]++;
	}

	//find out the numerical value for each starting code
	uint32_t* next_code = calloc((array_max + 1), sizeof(uint32_t));
	uint32_t code = 0;
    bl_count[0] = 0;
    for (uint32_t bits = 1; bits <= array_max; bits++) 
	{
        code = (code + bl_count[bits-1]) << 1;
        next_code[bits] = code;
    }

	free(bl_count);

	return next_code;
}

//make tree given a list of code lengths
node* create_dynamic_tree(uint32_t* code_lengths, uint32_t num_codes)
{
	node* to_return = create_node();
	uint32_t* next_code = generate_codes(code_lengths, num_codes);
	
	//assign values to tree
	for(uint32_t i = 0; i < num_codes; i++)
	{	
		uint32_t length = code_lengths[i];

		//exclude code lengths that are 0
		if(length > 0)
		{
			add_node(to_return, i, reverse_bits(next_code[length], length), length);
			next_code[length]++;
		}
	}
	free(next_code);

	return to_return;
}

//this has to be a special function because of the strange indexes on the code length alphabet
node *create_alphabet(uint32_t *code_lengths, uint32_t num_codes)
{
	node *to_return = create_node();
	uint32_t* next_code = generate_codes(code_lengths, num_codes);

	uint32_t biggest_length = 0;
	for(int i = 0; i < num_codes; i++)
	{
		if(code_lengths[i] > biggest_length)
		{
			biggest_length = code_lengths[i];
		}
	}

	//this is a workaround to the strange code length order, assigns codes in the proper order
	for (int i = 0; i <= biggest_length; i++)
	{
		for (int x = 0; x < 19; x++)
		{
			if (code_lengths[alphabet_indexes[x]] == i && code_lengths[alphabet_indexes[x]] != 0)
			{
				add_node(to_return, x, reverse_bits(next_code[i], i), i);
				next_code[i] += 1;
			}
		}
	}
	free(next_code);

	return to_return;
}