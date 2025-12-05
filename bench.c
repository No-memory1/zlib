#include <stdio.h>
#include <time.h>
#include "zlib.h"
//압축 관련해서 테스트하기 위한 코드
static double now_ms(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
}

int main(int argc, char *argv[]) {
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    Bytef *src = malloc(size);
    fread(src, 1, size, f);
    fclose(f);

    uLongf out_size = compressBound(size);
    Bytef *out = malloc(out_size);

    double t1 = now_ms();
    compress(out, &out_size, src, size);
    double t2 = now_ms();

    printf("Original: %ld bytes\n", size);
    printf("Compressed: %lu bytes (%.2f%%)\n", out_size, (100.0*out_size/size));
    printf("Time passed: %.2f ms\n", t2-t1);
}
