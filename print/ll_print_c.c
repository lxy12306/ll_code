#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "ll_print_c.h"

#define MOD_SIZE 0x10
#define MOD_WIDTH 8

void ll_fprint_vec_mod(FILE *output_file, int num_of_space, int size) {
	const char* s_template = "%%-%dd";
	char ac_buffer[MOD_SIZE];
	int i = 0;
	sprintf(ac_buffer,s_template, num_of_space);

	for(; i < size; ++i) {
		fprintf(output_file, ac_buffer, i);
	}
	fprintf(output_file, "\n");
}

size_t ll_fprint_vec_oneline(FILE* output_file, const char* mem,const char* mode, int mode_width, int i) {
	char ac_buffer[MOD_SIZE];
	const char *s_template = "%%#-%d%s";
	sprintf(ac_buffer,s_template, mode_width, mode);

	
	int step = 0;	
	
	switch(*mode) {
		case 'c': {
			const char *temp = (const char *)mem;
			for(int j = 0; j < i; ++j, ++temp) {
				fprintf(output_file, ac_buffer, *temp);
			}
			step = 0;	
		}
		break;
		case 'b': {
			ac_buffer[strlen(ac_buffer) - 1] = 'x';
			const uint8_t *temp = (const uint8_t *)mem;
			for(int j = 0; j < i; ++j, ++temp) {
				fprintf(output_file, ac_buffer, *temp);
			}
			step = 0;
		}
		break;
		case 'x':
		case 'd':{
			const int *temp = (const int *)mem;
			for(int j = 0; j < i; ++j, ++temp) {
				fprintf(output_file, ac_buffer, *temp);
			}
			step = 2;
		}
		break;
		default:{
			if (strcmp(mode, "lld") ==0 || strcmp(mode, "llx") == 0) {
				const long long *temp = (const long long *)mem;
				for(int j = 0; j < i; ++j, ++temp) {
					fprintf(output_file, ac_buffer, *temp);
				}
				step = 3;
			}
		}
		break;
	}
	fprintf(output_file, "\n");
	return i << step;
}
const char* ll_fprint_vec_one(FILE* output_file, const char* mem,const char* mode, int mode_width, int i) {
	//fprintf(output_file, "%*s", mode_width, "");
	ll_fprint_vec_mod(output_file, mode_width, i);
	//fprintf(output_file, "%*s", mode_width, "");
	int step = ll_fprint_vec_oneline(output_file, mem, mode, mode_width, i);
	
	return mem + step;
}
#define FIRST_COLUMN_WIDTH 5

const char* ll_fprint_vec_two(FILE *output_file, const char *mem,const char* mode, int mode_width, int i, int j) {
	fprintf(output_file, "%*s", FIRST_COLUMN_WIDTH, "");	
	ll_fprint_vec_mod(output_file, mode_width, j);

	int step = 0;

	switch (*mode) {
		case 'c':
		case 'b': {
			step = 0;
		}
		break;
		case 'x':
		case 'd': {
			step = 2;
		}
		break;
		default: {
			if (strcmp(mode, "lld") ==0 || strcmp(mode, "llx") == 0) {
				step = 3;
			}
		}
		break;
	}
	
	step = j << step;

	for (int k = 0; k < i; ++k) {
		fprintf(output_file, "%-*d", FIRST_COLUMN_WIDTH, k);
		ll_fprint_vec_oneline(output_file, mem, mode, mode_width, j);
		fprintf(output_file, "\n");
		mem += step;
	}

	return mem;
}

const char * ll_fprint_vec_two_bound(FILE *output_file, const char *mem,const char* mode, int mode_width, int i, int lengh) {
	int j = lengh / i;
	
	mem = ll_fprint_vec_two(output_file, mem, mode, mode_width, j, i);
	
	fprintf(output_file, "%-*d", FIRST_COLUMN_WIDTH, j);	
	
	j = lengh % i;
	int step = ll_fprint_vec_oneline(output_file, mem, mode, mode_width, j);
	return mem + step;
}
