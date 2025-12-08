#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "zlib.h"

#define CHUNK 16384

/* 진행률 표시 */
static void print_progress(z_stream *strm,
                           unsigned long long total_in_bytes,
                           int *last_percent)
{
    if (total_in_bytes == 0) return;

    unsigned long long now = strm->total_in;
    int percent = (int)(now * 100 / total_in_bytes);

    if (percent != *last_percent) {
        *last_percent = percent;
        fprintf(stderr, "\rcompressing... %3d%%", percent);
        fflush(stderr);
    }
}

/* 공통 압축 루틴: init_func만 바꿔 호출 */
static int run_deflate(FILE *source, FILE *dest,
                       int (*init_func)(z_streamp strm),
                       unsigned long long *total_in,
                       unsigned long long *total_out,
                       double *elapsed_sec)
{
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    int ret;
    int flush;
    unsigned have;

    /* 파일 크기 구하기 */
    long cur = ftell(source);
    long end;

    if (cur != -1L && fseek(source, 0, SEEK_END) == 0) {
        end = ftell(source);
        if (end > 0)
            *total_in = (unsigned long long)end;
        fseek(source, cur, SEEK_SET);
    }

    clock_t start = clock();

    memset(&strm, 0, sizeof(strm));

    /* 핵심: init_func에 따라 Init2 또는 Init2new 적용됨 */
    ret = init_func(&strm);
    if (ret != Z_OK) return ret;

    int last_percent = -1;
    *total_out = 0;

    /* 압축 루프 */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            deflateEnd(&strm);
            return Z_ERRNO;
        }

        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        do {
            strm.avail_out = CHUNK;
            strm.next_out  = out;

            ret = deflate(&strm, flush);
            if (ret == Z_STREAM_ERROR) {
                deflateEnd(&strm);
                return ret;
            }

            have = CHUNK - strm.avail_out;
            *total_out += have;

            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                deflateEnd(&strm);
                return Z_ERRNO;
            }

            print_progress(&strm, *total_in, &last_percent);

        } while (strm.avail_out == 0);

    } while (flush != Z_FINISH);

    deflateEnd(&strm);

    clock_t end_time = clock();
    *elapsed_sec = (double)(end_time - start) / CLOCKS_PER_SEC;

    fprintf(stderr, "\n");

    return Z_OK;
}

/* 기존 deflateInit2 */
int init_old(z_streamp strm)
{
    return deflateInit2(strm,
                        Z_DEFAULT_COMPRESSION,
                        Z_DEFLATED,
                        15 + 16,  /* gzip */
                        8,
                        Z_DEFAULT_STRATEGY);
}

/* 새 deflateInit2new */
int init_new(z_streamp strm)
{
    return deflateInit2new(strm,
                           Z_DEFAULT_COMPRESSION,
                           Z_DEFLATED,
                           15 + 16,
                           8,
                           Z_DEFAULT_STRATEGY);
}

static void print_result(const char *title,
                         unsigned long long total_in,
                         unsigned long long total_out,
                         double elapsed)
{
    double ratio = 1.0 - ((double)total_out / (double)total_in);
    double speed = (total_in / 1024.0 / 1024.0) / elapsed;

    printf("\n=== %s ===\n", title);
    printf("Original size:   %llu bytes\n", total_in);
    printf("Compressed size: %llu bytes\n", total_out);
    printf("Compression ratio: %.2f%%\n", ratio * 100);
    printf("Speed: %.2f MB/s\n", speed);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        printf("usage: %s input old.gz new.gz\n", argv[0]);
        return 1;
    }

    const char *input = argv[1];
    const char *out_old = argv[2];
    const char *out_new = argv[3];

    /* === 1) 기존 deflateInit2() 테스트 === */
    FILE *in1 = fopen(input, "rb");
    FILE *o1 = fopen(out_old, "wb");

    unsigned long long in_size1 = 0, out_size1 = 0;
    double time1 = 0;

    printf("Running old zlib deflateInit2()...\n");
    int ret1 = run_deflate(in1, o1, init_old, &in_size1, &out_size1, &time1);

    fclose(in1);
    fclose(o1);

    if (ret1 != Z_OK) {
        printf("Old compression failed: %d\n", ret1);
        return 1;
    }

    print_result("Original deflateInit2()", in_size1, out_size1, time1);

    /* === 2) 새 deflateInit2new() 테스트 === */
    FILE *in2 = fopen(input, "rb");
    FILE *o2 = fopen(out_new, "wb");

    unsigned long long in_size2 = 0, out_size2 = 0;
    double time2 = 0;

    printf("\nRunning new deflateInit2new()...\n");
    int ret2 = run_deflate(in2, o2, init_new, &in_size2, &out_size2, &time2);

    fclose(in2);
    fclose(o2);

    if (ret2 != Z_OK) {
        printf("New compression failed: %d\n", ret2);
        return 1;
    }

    print_result("New deflateInit2new()", in_size2, out_size2, time2);

    return 0;
}

