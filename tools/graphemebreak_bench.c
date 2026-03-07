/* Benchmark grapheme break on a UTF-8 text file.
 *
 * Compilation:
 *   cc graphemebreak_bench.c -lunibreak
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <graphemebreak.h>
#include <unibreakbase.h>

int main(int argc, char *argv[])
{
    const char *path;
    FILE *fp;
    char *text;
    char *breaks;
    size_t len;
    size_t read_len;
    int repeat = 10;
    int i;
    clock_t t0;
    clock_t t1;
    double elapsed;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "Usage: %s <utf8_file> [repeat]\n", argv[0]);
        return 1;
    }

    path = argv[1];
    if (argc == 3)
    {
        repeat = atoi(argv[2]);
        if (repeat <= 0)
        {
            fprintf(stderr, "Invalid repeat count\n");
            return 1;
        }
    }

    fp = fopen(path, "rb");
    if (!fp)
    {
        perror("Cannot open file");
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        perror("fseek failed");
        fclose(fp);
        return 1;
    }
    len = (size_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        perror("fseek failed");
        fclose(fp);
        return 1;
    }

    text = (char *)malloc(len + 1);
    breaks = (char *)malloc(len);
    if (!text || !breaks)
    {
        fprintf(stderr, "Out of memory\n");
        fclose(fp);
        free(text);
        free(breaks);
        return 1;
    }

    read_len = fread(text, 1, len, fp);
    fclose(fp);
    if (read_len != len)
    {
        fprintf(stderr, "Short read\n");
        free(text);
        free(breaks);
        return 1;
    }
    text[len] = '\0';

    t0 = clock();
    for (i = 0; i < repeat; ++i)
    {
        set_graphemebreaks_utf8((const utf8_t *)text, len, "", breaks);
    }
    t1 = clock();

    elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("bytes=%zu repeat=%d elapsed=%.6f s throughput=%.2f MiB/s\n",
           len, repeat, elapsed,
           (len * (double)repeat) / (1024.0 * 1024.0 * elapsed));

    free(text);
    free(breaks);
    return 0;
}
