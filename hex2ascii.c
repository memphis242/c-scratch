#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <errno.h>
#include <assert.h>

#define ARRLEN(arr) ( sizeof(arr) / sizeof(arr[0]) )

constexpr size_t MAX_HEX_STRLEN = 1000;

int main ( int argc, char * argv[] )
{
   if ( argc < 2 )
   {
      (void)fprintf(stderr, "Missing hex argument\n");
      return -1;
   }
   else if ( argc > 2 )
   {
      (void)fprintf(stderr, "Unexpected extra arguments: %d\n", argc - 2);
      return -2;
   }
   else if ( strnlen(argv[1], MAX_HEX_STRLEN+1) >= MAX_HEX_STRLEN+1 )
   {
      (void)fprintf(stderr, "Hex string greater than supported len: %zu\n", MAX_HEX_STRLEN);
      return -3;
   }

   const char * currhex = &argv[1][0];

   // Skip "0x" or "x" or "X" prefix...
   if ( *currhex == '0' && (*(currhex + 1) == 'x' || *(currhex + 1) == 'X') )
   {
      currhex += 2;
   }
   else if ( *currhex == 'X' || *currhex == 'x' )
   {
      currhex += 1;
   }

   size_t hexlen = strlen(currhex);
   assert(hexlen <= MAX_HEX_STRLEN);

   if ( hexlen % 2 != 0 )
   {
      (void)fprintf(stderr, "Invalid hex encoding: Odd number of hex digits: %zu\n", hexlen);
      return -4;
   }

   char ascii[ ((hexlen + 1) / 2) + 1];
   for ( size_t i = 0; i < ARRLEN(ascii); i++ )
      ascii[i] = '\0';

   assert(currhex != nullptr);
   for ( size_t i = 0;
         i < ARRLEN(ascii) && currhex != nullptr && *currhex != '\0';
         currhex += 2, i++ )
   {
      char pair[3];
      pair[0] = *currhex;
      pair[1] = *(currhex + 1);
      pair[2] = '\0';

      char * endptr = &pair[0];
      long int nextval = strtol(pair, &endptr, 16);
      if ( *endptr != '\0' )
      {
         (void)fprintf(stderr,
                  "strtol() went wrong, or invalid hex-pair detected: %s\n"
                  "strtol() returned %ld, errno: %s (%d): %s\n",
                  pair, nextval,
                  strerrorname_np(errno), errno, strerror(errno) );
         return 1;
      }
      
      assert(nextval >= 0x00);
      assert(nextval <= 0xFF);

      ascii[i] = (char)nextval;
   }

#ifndef NDEBUG
   // Assert that ascii is nul-terminated
   bool nulterminated = false;
   for ( size_t i = 0; i < ARRLEN(ascii); i++ )
   {
      if ( '\0' == ascii[i] )
      {
         nulterminated = true;
         break;
      }
   }

   assert(nulterminated);
#endif

   (void)printf("%s\n", ascii);

   return 0;
}
