/* Read an arbitrary number of unsigned integers (up to 64 significant bits)
 * from standard input and write the pattern-delimited gbproto encoding of them
 * to standard output. */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
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

static void die(char const *msg, ...) {
   va_list arg;
   (void)fputs("An error occurred: ", stderr);
   va_start(arg, msg);
   (void)vfprintf(stderr, msg, arg);
   va_end(arg);
   (void)fputc('\n', stderr);
   exit(EXIT_FAILURE);
}

/* Pack the base-256 encoded binary big-endian unsigned integer from <inbuf>
 * (which is <ilen> bytes long) into the array <outbuf> (which is <olen> bytes
 * long).
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

static void chkinp(void) {
   if (ferror(stdin)) die("Error reading from standard input!");
}

/* A callback function for reading more bytes from an input source. At
 * least one byte will be requested for reading. */
typedef void (*byte_reader)(void *dest, size_t bytes, void *related_data);

/* Read a complete encoding into the input buffer <inbuf> (which is <ilen>
 * bytes long) and decode it into a base-256 encoded binary big-endian unsiged
 * integer into output buffer <obuf> (which is <olen> bytes long).
 *
 * The output buffer will be padded with leading zeroes as necessary.
 *
 * <byte_reader> is a callback function which needs to read more bytes from
 * the input source. <related_data> will be passed through uninterpreted to
 * the callback and can be used or ignored by it in any way desirable.
 * The callback must only return if successful, otherwise it should
 * exit the program or perform a non-local jump to some error handler.
 *
 * Returns the size of the complete encoding in <inbuf>. */
static size_t unpack_pattern_delimited(
      char *inbuf, size_t ilen, char *outbuf, size_t olen
   ,  byte_reader callback, void *related_data
) {
   size_t i, total, missing, o;
   uint8_t *in;
   uint8_t mask, octet, *out;
   assert(inbuf != outbuf);
   assert(ilen);
   assert(olen);
   in= (uint8_t *)inbuf;
   out= (uint8_t *)outbuf;
   /* Find the first octet not equal to 0xff. This will allow us to calculate
    * the required input buffer size for the whole encoding. */
   i= 0;
   do {
      /* Read the next octet. */
      assert(i < ilen);
      #ifndef NDEBUG
         /* Enforce assertion failure later if callback does nothing. */
         in[i]= 0xff;
      #endif
      (*callback)(in + i, 1, related_data);
   } while ((octet= in[i++]) == 0xff);
   total= (i << 3) + 1;
   /* Find leftmost zero bit which terminates the prefix mask. */
   for (mask= 0x80; octet & mask; ++total) {
      mask>>= 1;
      assert(mask);
   }
   assert(total <= ilen);
   /* Now that the total size is known, read the rest (if any). */
   o= olen;
   if (missing= total - i) {
      #ifndef NDEBUG
         /* Create a huge number if callback dares to read nothing. */
         (void)memset(in + i, missing, 0xaa);
      #endif
      (*callback)(in + i, missing, related_data);
      /* Also copy the missing data to the end of <outbuf>. */
      assert(o > missing); /* We have already read at least one octet. */
      (void)memcpy(out + (o-= missing), in + i, missing);
   }
   /* Clear pattern prefix in first octet of value. */
   assert(o);
   out[--o]= octet & ~mask;
   /* Pad the rest of <outbuf> with binary leading zeroes. */
   while (o) out[--o]= 0;
   return total;
}

static void read_callback(void *dest, size_t bytes, void *related_data) {
   (void)related_data;
   if (fread(dest, bytes, 1, stdin) != 1) {
      chkinp();
      if (feof(stdin)) die("Unexpected end-of-file encountered!");
   }
}

int main(int argc, char **argv) {
   uint8_t buf[(64 + (7 - 1)) / 7];
   uint_fast64_t num;
   if (argc != 2) usage: die("Usage: %s (-e | -d)", argv[0]);
   if (!strcmp(argv[1], "-e")) {
      while (fscanf(stdin, "%" SCNuFAST64, &num) == 1) {
         char *start;
         TOGGLE_BIG_ENDIAN_OR_NATIVE(num);
         start= pack_pattern_delimited(
            (char const *)&num, sizeof num, buf, sizeof buf
         );
         if (
            fwrite(start, (char *)buf + sizeof buf - start, 1, stdout) != 1
         ) {
            goto wr_err;
         }
      }
      chkinp();
      if (!feof(stdin)) {
         die("Unrecognized trailing garbage on standard input!");
      }
   } else if (!strcmp(argv[1], "-d")) {
      int c;
      while ((c= getchar()) != EOF) {
         if (ungetc(c, stdin) == EOF) die("Internal error!");
         (void)unpack_pattern_delimited(
            buf, sizeof buf, (char *)&num, sizeof num, &read_callback, 0
         );
         TOGGLE_BIG_ENDIAN_OR_NATIVE(num);
         if (printf("%" PRIuFAST64 "\n", num) < 0) goto wr_err;
      }
      chkinp();
      assert(feof(stdin));
   } else {
      goto usage;
   }
   if (fflush(0)) {
      wr_err: die("Failure writing to standard output!");
   }
   return EXIT_SUCCESS;
}
