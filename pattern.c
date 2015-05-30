#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

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
      (void)printf("Got %" PRIuFAST64 ".\n", inbuf);
   }
   if (ferror(stdin)) die("Error reading from standard input!");
   if (!feof(stdin)) die("Unrecognized trailing garbage on standard input!");
}
