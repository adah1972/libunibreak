/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Tests for incomplete UTF-8/16 input.
 *
 * Copyright (C) 2026 Wu Yongwei <wuyongwei at gmail dot com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software.  If you use this
 *    software in a product, an acknowledgement in the product
 *    documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file    tests_incomplete.c
 *
 * Regression tests for incomplete UTF-8/16 sequences.
 *
 * @author  Wu Yongwei
 */

#include <stdio.h>
#include <string.h>
#include "linebreak.h"
#include "wordbreak.h"
#include "graphemebreak.h"

#define CANARY 0x5A

static int failures;

static void expect_bytes(
        const char *test, const char *actual, const char *expected,
        size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i)
    {
        if ((unsigned char)actual[i] != (unsigned char)expected[i])
        {
            fprintf(stderr, "%s: position %zu: expected %d, got %d\n",
                    test, i, (unsigned char)expected[i],
                    (unsigned char)actual[i]);
            ++failures;
            return;
        }
    }
}

static void expect_size(const char *test, size_t actual, size_t expected)
{
    if (actual != expected)
    {
        fprintf(stderr, "%s: expected %zu, got %zu\n",
                test, expected, actual);
        ++failures;
    }
}

static void check_guards(const char *test, const char *buf, size_t len)
{
    if ((unsigned char)buf[0] != CANARY)
    {
        fprintf(stderr, "%s: write before output buffer\n", test);
        ++failures;
    }
    if ((unsigned char)buf[len + 1] != CANARY)
    {
        fprintf(stderr, "%s: write after output buffer\n", test);
        ++failures;
    }
}

static void test_utf8_incomplete(void)
{
    static const utf8_t empty[] = { 0 };
    static const utf8_t truncated[] = { 0xE2, 0x82 };
    static const utf8_t complete_tail[] = { 0x61, 0xE2, 0x82 };
    static const char line_truncated[] = {
        LINEBREAK_INSIDEACHAR, LINEBREAK_INSIDEACHAR
    };
    static const char word_truncated[] = {
        WORDBREAK_INSIDEACHAR, WORDBREAK_INSIDEACHAR
    };
    static const char grapheme_truncated[] = {
        GRAPHEMEBREAK_INSIDEACHAR, GRAPHEMEBREAK_INSIDEACHAR
    };
    static const char line_complete_tail[] = {
        LINEBREAK_INDETERMINATE, LINEBREAK_INSIDEACHAR,
        LINEBREAK_INSIDEACHAR
    };
    static const char word_complete_tail[] = {
        WORDBREAK_BREAK, WORDBREAK_INSIDEACHAR, WORDBREAK_INSIDEACHAR
    };
    static const char grapheme_complete_tail[] = {
        GRAPHEMEBREAK_BREAK, GRAPHEMEBREAK_INSIDEACHAR,
        GRAPHEMEBREAK_INSIDEACHAR
    };
    char out[8];
    size_t count;

    memset(out, CANARY, sizeof out);
    set_linebreaks_utf8(empty, 0, "", out + 1);
    check_guards("line: empty UTF-8", out, 0);

    memset(out, CANARY, sizeof out);
    set_wordbreaks_utf8(empty, 0, "", out + 1);
    check_guards("word: empty UTF-8", out, 0);

    memset(out, CANARY, sizeof out);
    set_graphemebreaks_utf8(empty, 0, "", out + 1);
    check_guards("grapheme: empty UTF-8", out, 0);

    memset(out, CANARY, sizeof out);
    set_linebreaks_utf8(truncated, 2, "", out + 1);
    expect_bytes("line: truncated UTF-8", out + 1, line_truncated, 2);
    check_guards("line: truncated UTF-8", out, 2);

    memset(out, CANARY, sizeof out);
    set_wordbreaks_utf8(truncated, 2, "", out + 1);
    expect_bytes("word: truncated UTF-8", out + 1, word_truncated, 2);
    check_guards("word: truncated UTF-8", out, 2);

    memset(out, CANARY, sizeof out);
    set_graphemebreaks_utf8(truncated, 2, "", out + 1);
    expect_bytes("grapheme: truncated UTF-8", out + 1,
                 grapheme_truncated, 2);
    check_guards("grapheme: truncated UTF-8", out, 2);

    memset(out, CANARY, sizeof out);
    set_linebreaks_utf8(complete_tail, 3, "", out + 1);
    expect_bytes("line: complete UTF-8 plus tail", out + 1,
                 line_complete_tail, 3);
    check_guards("line: complete UTF-8 plus tail", out, 3);

    memset(out, CANARY, sizeof out);
    set_wordbreaks_utf8(complete_tail, 3, "", out + 1);
    expect_bytes("word: complete UTF-8 plus tail", out + 1,
                 word_complete_tail, 3);
    check_guards("word: complete UTF-8 plus tail", out, 3);

    memset(out, CANARY, sizeof out);
    set_graphemebreaks_utf8(complete_tail, 3, "", out + 1);
    expect_bytes("grapheme: complete UTF-8 plus tail", out + 1,
                 grapheme_complete_tail, 3);
    check_guards("grapheme: complete UTF-8 plus tail", out, 3);

    memset(out, CANARY, sizeof out);
    count = set_linebreaks_utf8_per_code_point(truncated, 2, "",
                                               out + 1);
    expect_size("line per code point: truncated UTF-8", count, 0);
    check_guards("line per code point: truncated UTF-8", out, 0);

    memset(out, CANARY, sizeof out);
    count = set_linebreaks_utf8_per_code_point(complete_tail, 3, "",
                                               out + 1);
    expect_size("line per code point: complete UTF-8 plus tail",
                count, 1);
    expect_bytes("line per code point: complete UTF-8 plus tail",
                 out + 1, line_complete_tail, 1);
    check_guards("line per code point: complete UTF-8 plus tail",
                 out, 1);
}

static void test_utf16_incomplete(void)
{
    static const utf16_t truncated[] = { 0xD800 };
    static const utf16_t complete_tail[] = { 0x0061, 0xD800 };
    static const char line_truncated[] = { LINEBREAK_INSIDEACHAR };
    static const char word_truncated[] = { WORDBREAK_INSIDEACHAR };
    static const char grapheme_truncated[] = {
        GRAPHEMEBREAK_INSIDEACHAR
    };
    static const char line_complete_tail[] = {
        LINEBREAK_INDETERMINATE, LINEBREAK_INSIDEACHAR
    };
    static const char word_complete_tail[] = {
        WORDBREAK_BREAK, WORDBREAK_INSIDEACHAR
    };
    static const char grapheme_complete_tail[] = {
        GRAPHEMEBREAK_BREAK, GRAPHEMEBREAK_INSIDEACHAR
    };
    char out[8];
    size_t count;

    memset(out, CANARY, sizeof out);
    set_linebreaks_utf16(truncated, 1, "", out + 1);
    expect_bytes("line: truncated UTF-16", out + 1,
                 line_truncated, 1);
    check_guards("line: truncated UTF-16", out, 1);

    memset(out, CANARY, sizeof out);
    set_wordbreaks_utf16(truncated, 1, "", out + 1);
    expect_bytes("word: truncated UTF-16", out + 1,
                 word_truncated, 1);
    check_guards("word: truncated UTF-16", out, 1);

    memset(out, CANARY, sizeof out);
    set_graphemebreaks_utf16(truncated, 1, "", out + 1);
    expect_bytes("grapheme: truncated UTF-16", out + 1,
                 grapheme_truncated, 1);
    check_guards("grapheme: truncated UTF-16", out, 1);

    memset(out, CANARY, sizeof out);
    set_linebreaks_utf16(complete_tail, 2, "", out + 1);
    expect_bytes("line: complete UTF-16 plus tail", out + 1,
                 line_complete_tail, 2);
    check_guards("line: complete UTF-16 plus tail", out, 2);

    memset(out, CANARY, sizeof out);
    set_wordbreaks_utf16(complete_tail, 2, "", out + 1);
    expect_bytes("word: complete UTF-16 plus tail", out + 1,
                 word_complete_tail, 2);
    check_guards("word: complete UTF-16 plus tail", out, 2);

    memset(out, CANARY, sizeof out);
    set_graphemebreaks_utf16(complete_tail, 2, "", out + 1);
    expect_bytes("grapheme: complete UTF-16 plus tail", out + 1,
                 grapheme_complete_tail, 2);
    check_guards("grapheme: complete UTF-16 plus tail", out, 2);

    memset(out, CANARY, sizeof out);
    count = set_linebreaks_utf16_per_code_point(truncated, 1, "",
                                                out + 1);
    expect_size("line per code point: truncated UTF-16", count, 0);
    check_guards("line per code point: truncated UTF-16", out, 0);

    memset(out, CANARY, sizeof out);
    count = set_linebreaks_utf16_per_code_point(complete_tail, 2, "",
                                                out + 1);
    expect_size("line per code point: complete UTF-16 plus tail",
                count, 1);
    expect_bytes("line per code point: complete UTF-16 plus tail",
                 out + 1, line_complete_tail, 1);
    check_guards("line per code point: complete UTF-16 plus tail",
                 out, 1);
}

int main(void)
{
    test_utf8_incomplete();
    test_utf16_incomplete();

    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }

    printf("Incomplete-input tests passed\n");
    return 0;
}
