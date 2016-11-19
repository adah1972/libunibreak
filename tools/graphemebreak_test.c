/* Simple program that read the grapheme break test database of the unicode standard
 * and checks the output of libunibreak against the desired values of the test file
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <graphemebreak.h>

static int hex[256] = {
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
   0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

int
main(int argc, char *argv[])
{
  FILE *fp = fopen("GraphemeBreakTest.txt", "r");
  int i;
  char line[500];
  char breaks_desired[500];
  char breaks_actual[500];
  utf32_t txt[500];
  int linenumber = 1;

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
      if (line[i] == '#') line[i] = 0;
    }

    l = strlen(line);

    // skip empty lines
    if (l >= 5)
    {
      for (i = 0; i < 500; i++)
      {
        txt[i] = 0;
      }

      for (i = 3; i < l; i++)
      {
        switch(line[i])
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
            txt[p] = txt[p] * 16 + hex[line[i]];
            break;

          case '\xB7':
            breaks_desired[p] = GRAPHEMEBREAK_BREAK;
            p++;
            break;
          case '\x97':
            breaks_desired[p] = GRAPHEMEBREAK_NOBREAK;
            p++;
            break;

          default:
            // ignore this character
            break;
        }
      }

      set_graphemebreaks_utf32(txt, p, "", breaks_actual);

      for (i = 0; i < p-1; i++)
        if (breaks_actual[i] != breaks_desired[i])
          printf("problem with line %i %s\n", linenumber, line);
    }

    linenumber++;
  }

  fclose(fp);
}
