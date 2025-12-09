#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "zlib.h"

#define CHUNK 16384  /* 한 번에 읽고 쓰는 버퍼 크기 */

/* 압축 진행률(%)을 stderr로 출력 */
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

/* 파일 압축 및 통계 계산 */
static int run_deflate(FILE *source, FILE *dest,
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

    /* 입력 파일 크기 구하기 */
    long cur = ftell(source);
    long end;
    *total_in = 0;
    if (cur != -1L && fseek(source, 0, SEEK_END) == 0) {
        end = ftell(source);
        if (end > 0)
            *total_in = (unsigned long long)end;
        fseek(source, cur, SEEK_SET);
    }

    clock_t start = clock();

    /* z_stream 초기화 */
    memset(&strm, 0, sizeof(strm));
    ret = deflateInit2(&strm,
                       5,
                       Z_DEFLATED,
                       15 + 16, /* gzip 포맷 */
                       8,
                       Z_DEFAULT_STRATEGY);
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
            strm.next_out = out;

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

    if (*total_in > 0) fprintf(stderr, "\n");
    return Z_OK;
}

/* 결과 출력 */
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
    if (argc < 3) {
        printf("usage: %s <input> <output.gz>\n", argv[0]);
        return 1;
    }

    const char *input = argv[1];
    const char *output = argv[2];

    FILE *in = fopen(input, "rb");
    if (!in) {
        perror("open input");
        return 1;
    }
    FILE *out = fopen(output, "wb");
    if (!out) {
        perror("open output");
        fclose(in);
        return 1;
    }

    unsigned long long in_size = 0, out_size = 0;
    double elapsed = 0;

    int ret = run_deflate(in, out, &in_size, &out_size, &elapsed);

    fclose(in);
    fclose(out);

    if (ret != Z_OK) {
        fprintf(stderr, "compression failed: %d\n", ret);
        return 1;
    }

    print_result("Compression Result", in_size, out_size, elapsed);

    return 0;
}

