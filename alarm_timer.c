// Alarm timer headers
#include <unistd.h>
#include <signal.h>
// General-purpose headers
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>

constexpr int SIGACTION_SUCCESS = 0;

static volatile sig_atomic_t bAlarm = false;

void handleSIGALRM(int signum)
{
   (void)signum; // Only one signal associated to this handler
   bAlarm = true;
}

enum MainRetCode
{
   MAIN_RETCODE_FINE,
   MAIN_RETCODE_MISSING_ARGS,
   MAIN_RETCODE_MALFORMED_ARG,
   MAIN_RETCODE_TIME_OOB,
   MAIN_RETCODE_SIGACTION_FAILED,
   MAIN_RETCODE_FAILED_TO_ALARM,
};

int main(int argc, char * argv[])
{
   // Check argument count correctness
   if ( argc != 2 )
   {
      fprintf( stderr, "Need to pass in a time argument in seconds. Please try again.\n");
      return (int)MAIN_RETCODE_MISSING_ARGS;
   }

   // Attempt to parse time argument
   const size_t arglen = strlen(argv[1]);
   assert(arglen > 0);
   char * endptr = argv[1] + arglen;
   const long int seconds = strtol( argv[1], &endptr, 10 );
   assert( endptr >= argv[1] );
   assert( endptr <= argv[1] + arglen );
   if ( *endptr != '\0' )
   {
      fprintf( stderr,
               "Invalid time argument: %s. strtol() detected invalid char at %td : %c",
               argv[1], (ptrdiff_t)(endptr - argv[1]), *endptr );

      return (int)MAIN_RETCODE_MALFORMED_ARG;
   }
   else if ( LONG_MIN == seconds || LONG_MAX == seconds )
   {
      fprintf( stderr,
               "Time amount out-of-bounds: %s. strtol() returned: %ld. errno: %s (%d)\n",
               argv[1], seconds, strerror(errno), errno );

      return (int)MAIN_RETCODE_TIME_OOB;
   }

   // Register signal handler
   struct sigaction sa_cfg;
   memset(&sa_cfg, 0x00, sizeof sa_cfg);
   sigemptyset(&sa_cfg.sa_mask);
   sa_cfg.sa_flags = 0; // defaults, including no SA_RESTART
   int retcode = sigaction(SIGALRM, &sa_cfg, NULL);
   if ( retcode != SIGACTION_SUCCESS )
   {
      fprintf( stderr,
               "sigaction(SIGALRM, &sa_cfg, NULL) failed! errno: %s (%d)",
               strerror(errno), errno );
      return (int)MAIN_RETCODE_SIGACTION_FAILED;
   }

   // Start the timer!
   (void)alarm(seconds); // return value doesn't matter...

   // Alarm
   while ( !bAlarm ); // wait for SIGALRM...
   retcode = printf("\a");
   if ( retcode < 0 )
   {
      fprintf( stderr,
               "Failed to trigger alarm! errno: %s (%d)\n",
               strerror(errno), errno );

      return MAIN_RETCODE_FAILED_TO_ALARM;
   }
   fflush(stdout);

   return (int)MAIN_RETCODE_FINE;
}
