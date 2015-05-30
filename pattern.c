#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define SWAP_BIG_ENDIAN_OR_NATIVE(v) swab_inplace((char *)&(v), sizeof(v))

static void swab_inplace(char *buf, size_t bytes) {
   size_t i= 0;
   while (bytes > i + 1) {
      char t= buf[i];
      buf[i++]= buf[--bytes];
      buf[bytes]= t;
   }
}


static void die(char const *msg) {
   (void)fprintf(stderr, "An error occurred: %s\n", msg);
   exit(EXIT_FAILURE);
}

/* Read an arbitrary number of unsigned integers (up to 64 significant bits) from
 * standard input and write the pattern-delimited gbproto encoding of them to
 * standard output. */
int main(void) {
   uint8_t obuf[(64 + (7 - 1)) / 7];
   uint_fast64_t inbuf;
   while (fscanf(stdin, "%" SCNuFAST64, &inbuf) == 1) {
      SWAP_BIG_ENDIAN_OR_NATIVE(inbuf);
      if (fwrite(&inbuf, sizeof inbuf, 1, stdout) != 1) {
         wr_err: die("Failure writing to standard output!");
      }
   }
   if (ferror(stdin)) die("Error reading from standard input!");
   if (!feof(stdin)) die("Unrecognized trailing garbage on standard input!");
   if (fflush(0)) goto wr_err;
   return EXIT_SUCCESS;
}
