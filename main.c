#include <stdio.h>
#include <string.h>

int main(void)
{
   const char longstr[] = "This is a reallllllly long string for buf to hold!";
   printf("longstr: %s\n", longstr);

   char buf[10];
   int nchars_potential = snprintf(buf, sizeof(buf), "%-*s", 200, longstr);
   printf("int nchars_potential = snprintf(buf, sizeof(buf), \"%%-*s\", 200, longstr);\n");

   printf("buf: %s\n", buf);
   printf("buf (raw underlying bytes): \n     ");
   for ( size_t i=0; i < sizeof(buf); ++i )
      printf("0x%02X ", buf[i]);
   printf("\n");
   printf("nchars_potential: %d\n", nchars_potential);

   return 0;
}
