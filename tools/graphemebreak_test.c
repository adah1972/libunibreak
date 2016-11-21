/* Simple program that read the grapheme break test database of the unicode standard
 * and checks the output of libunibreak against the desired values of the test file
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <graphemebreak.h>
#include <assert.h>

#define LINE_LEN 1000


void utf32toutf8(utf32_t ch, char * result)
{
  if (ch < 0x80)
  {
    result[0] = (char)(ch);
    result[1] = 0;
  }
  // U+0080..U+07FF
  else if (ch < 0x800)
  {
    result[0] = (char)(0xC0 | (ch >> 6));
    result[1] = (char)(0x80 | (ch & 0x3F));
    result[2] = 0;
  }
  // U+0800..U+FFFF
  else if (ch < 0x10000)
  {
    result[0] = (char)(0xE0 | (ch >> 12));
    result[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
    result[2] = (char)(0x80 | (ch & 0x3F));
    result[3] = 0;
  }
  else if (ch < 0x110000)
  {
    result[0] = (char)(0xF0 | (ch >> 18));
    result[1] = (char)(0x80 | ((ch >> 12) & 0x3F));
    result[2] = (char)(0x80 | ((ch >> 6) & 0x3F));
    result[3] = (char)(0x80 | (ch & 0x3F));
    result[4] = 0;
  }
  else
  {
    assert(0);
  }
}



int
main(int argc, char *argv[])
{
  FILE *fp = fopen("GraphemeBreakTest.txt", "r");
  int i;
  char line[LINE_LEN];
  char breaks_desired[LINE_LEN];
  char breaks_desired_utf8[LINE_LEN];
  char breaks_actual[LINE_LEN];
  utf32_t txt[LINE_LEN];
  utf8_t txtutf8[LINE_LEN];
  int linenumber = 1;
  int result = 0;

  if (fp == 0)
  {
    fprintf(stderr, "Error opening file");
    return 1;
  }

  while(fgets(line, sizeof(line), fp))//no line will be longer than 100
  {
    // remove everything from the first # on

    int l = strlen(line);
    int p = 0;

    for (i = 0; i < l; i++)
    {
      if (line[i] == '#')
      {
        line[i] = 0;
        break;
      }
    }

    l = strlen(line);

    // only do lines that do contain a minimal amount of information
    if (l)
    {
      char * linepos = line;

      memset(txt, 0, LINE_LEN*sizeof(utf32_t));

      while (*linepos)
      {
        switch(*linepos)
        {
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
          case 'a':
          case 'b':
          case 'c':
          case 'd':
          case 'e':
          case 'f':
          case 'A':
          case 'B':
          case 'C':
          case 'D':
          case 'E':
          case 'F':
            txt[p] = strtol(linepos, &linepos, 16);
            p++;
            break;

          case '\xB7':
            breaks_desired[p] = GRAPHEMEBREAK_BREAK;
            linepos++;
            break;
          case '\x97':
            breaks_desired[p] = GRAPHEMEBREAK_NOBREAK;
            linepos++;
            break;

          default:
            // ignore this character
            linepos++;
            break;
        }
      }

      // the test data contains information for a break BEFORE the string
      // which we suppose is always a break, so fill that in and let the
      // breaking function start with the break after that one
      breaks_actual[0] = GRAPHEMEBREAK_BREAK;
      set_graphemebreaks_utf32(txt, p, "", breaks_actual+1);

      for (i = 0; i <= p; i++)
      {
        if (breaks_actual[i] != breaks_desired[i])
        {
          printf("problem with line %i pos %i %s (%i <--> %i)\n", linenumber, i, line, breaks_desired[i], breaks_actual[i]);
          result = 1;
          return result;
        }
      }

      // try the test again with utf-8 encoding, we need to convert our text
      // to uft8 for this, we also need to convert the desired array
      breaks_desired_utf8[0] = breaks_desired[0];
      txtutf8[0] = 0;
      for (i = 0; i < p; i++)
      {
        char utf8[5];
        utf32toutf8(txt[i], utf8);

        // store where the first new byte will be placed of the new utf-8 encoded character
        l = strlen(txtutf8) + 1;

        // append character
        strcat(txtutf8, utf8);

        // fill in the internal characters with INSIDECHAR
        while (l < strlen(txtutf8))
        {
          breaks_desired_utf8[l] = GRAPHEMEBREAK_INSIDECHAR;
          l++;
        }

        // copy the desired information
        breaks_desired_utf8[l] = breaks_desired[i+1];
      }

      breaks_actual[0] = GRAPHEMEBREAK_BREAK;
      set_graphemebreaks_utf8(txtutf8, l, "", breaks_actual+1);

      for (i = 0; i <= l; i++)
      {
        if (breaks_actual[i] != breaks_desired_utf8[i])
        {
          printf("problem with utf8 line %i pos %i %s (%i <--> %i)\n", linenumber, i, line, breaks_desired_utf8[i], breaks_actual[i]);
          result = 1;
          return result;
        }
      }
    }

    linenumber++;
  }

  fclose(fp);

  return result;
}
