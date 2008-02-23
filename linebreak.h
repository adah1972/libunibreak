/* vim: set tabstop=4 shiftwidth=4: */

/**
 * @file	linebreak.c
 *
 * Header file for the line breaking algorithm.
 *
 * @version	0.6, 2008/02/18
 * @author	Wu Yongwei
 */

#ifndef LINEBREAK_H
#define LINEBREAK_H

#ifndef LINEBREAK_UTF_TYPES_DEFINED
#define LINEBREAK_UTF_TYPES_DEFINED
typedef unsigned char	utf8_t;
typedef unsigned short	utf16_t;
typedef unsigned int	utf32_t;
#endif

#define LINEBREAK_MUSTBREAK		0	/**< Break is mandatory */
#define LINEBREAK_ALLOWBREAK	1	/**< Break is allowed */
#define LINEBREAK_NOBREAK		2	/**< No break is possible */
#define LINEBREAK_INSIDEACHAR	3	/**< A UTF-8/16 sequence is unfinished */

int is_breakable(utf32_t char1, utf32_t char2, const char* lang);
void set_linebreaks_utf8(
		const utf8_t *s, size_t len, const char* lang, char *brks);
void set_linebreaks_utf16(
		const utf16_t *s, size_t len, const char* lang, char *brks);
void set_linebreaks_utf32(
		const utf32_t *s, size_t len, const char* lang, char *brks);

#endif /* LINEBREAK_H */
