#ifndef _LL_PRINTF_C_H_
#define _LL_PRINTF_C_H_

#include <stdio.h>
#include <stdint.h>

void ll_fprint_vec(const char *mem,const char* mode, int dimension/*维度*/, int i, ...);

#define ll_print_vec_int(mem, i) ll_fprint_vec_one(stdout,(const char *)(mem), "d", 10, (i))
#define ll_print_vec_dw(mem, i) ll_fprint_vec_one(stdout,(const char *)(mem), "x", 12, (i))

#define ll_print_vec_char(mem, i) ll_fprint_vec_one(stdout, (const char *)(mem), "c", 4, (i))
#define ll_print_vec_b(mem, i) ll_fprint_vec_one(stdout, (const char *)(mem), "b", 6, (i))

#define ll_print_vec_ll(mem, i)  ll_fprint_vec_one(stdout, (const char *)(mem), "lld", 20, (i)）
#define ll_print_vec_qw(mem, i)  ll_fprint_vec_one(stdout, (const char *)(mem), "llx", 20, (i))
const char * ll_fprint_vec_one(FILE *output_file, const char *mem,const char* mode, int mode_width, int i);

#define ll_print_vecvec_int(mem, i, j) ll_fprint_vec_two(stdout,(const char *)(mem), "d", 10, (i), (j))
#define ll_print_vecvec_dw(mem, i, j) ll_fprint_vec_two(stdout,(const char *)(mem), "x", 12, (i), (j))

#define ll_print_vecvec_char(mem, i, j) ll_fprint_vec_two(stdout, (const char *)(mem), "c", 4, (i), (j))
#define ll_print_vecvec_b(mem, i, j) ll_fprint_vec_two(stdout, (const char *)(mem), "b", 6, (i), (j))

#define ll_print_vecvec_ll(mem, i, j)  ll_fprint_vec_two(stdout, (const char *)(mem), "lld", 20, (i), (j))
#define ll_print_vecvec_qw(mem, i, j)  ll_fprint_vec_two(stdout, (const char *)(mem), "llx", 20, (i), (j))
const char * ll_fprint_vec_two(FILE *output_file, const char *mem,const char* mode, int mode_width, int i, int j);

#define ll_print_vecvec_bound_int(mem, i, lengh) ll_fprint_vec_two_bound(stdout,(const char *)(mem), "d", 10, (i), (lengh))
#define ll_print_vecvec_bound_dw(mem, i, lengh) ll_fprint_vec_two_bound(stdout,(const char *)(mem), "x", 12, (i), (lengh))

#define ll_print_vecvec_bound_char(mem, i, lengh) ll_fprint_vec_two_bound(stdout, (const char *)(mem), "c", 4, (i), (lengh))
#define ll_print_vecvec_bound_b(mem, i, lengh) ll_fprint_vec_two_bound(stdout, (const char *)(mem), "b", 6, (i), (lengh))

#define ll_print_vecvec_bound_ll(mem, i, lengh)  ll_fprint_vec_two_bound(stdout, (const char *)(mem), "lld", 20, (i), (lengh))
#define ll_print_vecvec_bound_qw(mem, i, lengh)  ll_fprint_vec_two_bound(stdout, (const char *)(mem), "llx", 20, (i), (lengh))
const char * ll_fprint_vec_two_bound(FILE *output_file, const char *mem,const char* mode, int mode_width, int i, int lengh);
#endif //_LL_PRINTF_C_H_
