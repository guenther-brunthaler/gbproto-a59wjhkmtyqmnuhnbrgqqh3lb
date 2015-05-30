/* Read an arbitrary number of unsigned integers (up to 64 significant bits)
 * from standard input and write the pattern-delimited gbproto encoding of them
 * to standard output. */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
   #include "config.h"
#endif
#ifdef HAVE_INTTYPES_H
   #include <inttypes.h>
#endif

#if WORDS_BIGENDIAN
   #define TOGGLE_BIG_ENDIAN_OR_NATIVE(v)
#else
   #define TOGGLE_BIG_ENDIAN_OR_NATIVE(v) \
      swab_inplace((char *)&(v), sizeof(v))

   static void swab_inplace(char *buf, size_t bytes) {
      size_t i= 0;
      while (bytes > i + 1) {
         char t= buf[i];
         buf[i++]= buf[--bytes];
         buf[bytes]= t;
      }
   }
#endif

static void die(char const *msg) {
   (void)fprintf(stderr, "An error occurred: %s\n", msg);
   exit(EXIT_FAILURE);
}

/* Pack the base-256 encoded binary big-endian integer from <inbuf> (which
 * is <ilen> bytes long) into the array <outbuf> (which is <olen> bytes long).
 *
 * The packed output will be placed at the end of the output array, returning
 * the pointer into the array where the packed data starts. */
static char *pack_pattern_delimited(
   char const *inbuf, size_t ilen, char *outbuf, size_t olen
) {
   uint8_t const *in;
   uint8_t *out, mask, bit;
   size_t i, o, sig, plen, ffs;
   assert(CHAR_BIT == 8);
   assert(inbuf != outbuf);
   assert(olen >= ilen); assert(ilen > 0);
   /* At least the last byte is always significant. */
   sig= olen - 1;
   /* Copy verbatim and detect position of most significant byte. */
   in= (uint8_t const *)inbuf; out= (uint8_t *)outbuf;
   for (i= ilen , o= olen; i; ) {
      assert(i); assert(o);
      if (out[--o]= in[--i]) sig= o;
   }
   o= sig;
   recheck:
   plen= olen - o;
   assert(plen >= 1);
   /* Prefix the output with a 1...10 bit pattern <plen> bits wide, where
    * <plen> is the number of bytes in the whole output. */
   ffs= plen >> 3; /* Number of full 0xFF prefix bytes. */
   /* Number of remaining bits to prefix within the first value byte. */
   plen&= 7;
   /* Build prefix mask for first value byte. */
   bit= 0x80;
   for (mask= 0; plen--; bit>>= 1) mask|= bit;
   /* Check whether mask collides with bits of first value byte.
    *
    * The prefix to be written is the same as the mask, except for the
    * last "1"-bit of the mask which needs to be changed to a "0". */
   if (out[o] & mask) {
      /* It collides. Write to a new octet which gets prepended. */
      assert(o);
      out[--o]= 0;
      goto recheck;
   } else {
      /* No collision. Merge with first octet of value. */
      out[o]|= mask + mask & 0xff;
   }
   /* Now prefix the value with the remaining 0xff octets to be prepended. */
   while (ffs--) {
      assert(o);
      out[--o]= 0xff;
   }
   /* Finally, return a pointer to the start of the resulting encoding. */
   return (char *)out + o;
}

int main(void) {
   uint8_t obuf[(64 + (7 - 1)) / 7];
   uint_fast64_t inbuf;
   while (fscanf(stdin, "%" SCNuFAST64, &inbuf) == 1) {
      char *start;
      TOGGLE_BIG_ENDIAN_OR_NATIVE(inbuf);
      start= pack_pattern_delimited(
         (char const *)&inbuf, sizeof inbuf, obuf, sizeof obuf
      );
      if (fwrite(start, (char *)obuf + sizeof obuf - start, 1, stdout) != 1) {
         wr_err: die("Failure writing to standard output!");
      }
   }
   if (ferror(stdin)) die("Error reading from standard input!");
   if (!feof(stdin)) die("Unrecognized trailing garbage on standard input!");
   if (fflush(0)) goto wr_err;
   return EXIT_SUCCESS;
}
