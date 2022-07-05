// Filename: main.cpp
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
using namespace std;

void print(float* data, int n) {
  for (int i = 0; i < n; i ++) {
    printf("%f ", data[i]);
  }
  printf("\n");
}

void test_mm_insert_epi() {
    const int WIDTH = 128;
    int n = WIDTH/8/sizeof(int);
    char* w = (char*)aligned_alloc(64, sizeof(int)*n);
    __m128i s1;
    __m128i s2 =  _mm_insert_epi8(s1,10,1);
    memcpy(w,&s2, n*4);
}

int main() {
}