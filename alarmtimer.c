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

enum MainRetCode
{
   MAIN_RETCODE_FINE,
   MAIN_RETCODE_MISSING_ARGS,
   MAIN_RETCODE_MALFORMED_ARG,
   MAIN_RETCODE_TIME_OOB,
   MAIN_RETCODE_SLEEP_INTERRUPTED,
   MAIN_RETCODE_FAILED_TO_ALARM,
};

int main(int argc, char * argv[])
{
   int retcode;

   // Check argument correctness
   if ( argc != 2 )
   {
      fprintf( stderr,
               "Need to pass in a time argument in seconds.\n"
               "Please try again.\n" );
      return (int)MAIN_RETCODE_MISSING_ARGS;
   }

   const size_t arglen = strlen(argv[1]);
   // It'd be weird if the terminal passed in just "\0" for argv[1]
   assert(arglen > 0);

   // Attempt to parse time argument
   // Slight quirk /w strtol(): 0, LONG_MIN, and LONG_MAX may technically be
   // returned on fully valid input and invalid input, so, in order to clear
   // away any ambiguity, errno should be reset and then if 0, LONG_MIN, or
   // LONG_MAX are returned, errno can be checked for `ERANGE`
   errno = 0;
   char * endptr = nullptr;
   const long int seconds = strtol( argv[1], &endptr, 10 );

   // At this point, endptr should definitely be within certain bounds...
   assert( endptr >= argv[1] );
   assert( endptr <= argv[1] + arglen );

   if ( *endptr != '\0' )
   {
      fprintf( stderr,
               "Error: Invalid time argument: %s\n"
               "strtol() detected an invalid char at string idx %td : '%c'\n",
               argv[1],
               (ptrdiff_t)(endptr - argv[1]), *endptr );

      return (int)MAIN_RETCODE_MALFORMED_ARG;
   }
   else if ( ERANGE == errno )
   {
      fprintf( stderr,
               "Error: Time amount out-of-bounds: %s\n"
               "strtol() returned: %ld. errno: %s (%d)\n",
               argv[1],
               seconds, strerror(errno), errno );

      return (int)MAIN_RETCODE_TIME_OOB;
   }

   // Sleep for user's request time...
   unsigned int time_remaining = sleep(seconds);
   if ( time_remaining > 0 )
   {
      (void)fprintf( stderr,
               "sleep() was interrupted by a signal that was not masked.\n"
               "Time remaining: %u seconds\n",
               time_remaining );

      return MAIN_RETCODE_SLEEP_INTERRUPTED;
   }

   // ------------------------------ Alarm Time! ------------------------------
   // Unfortunately, the '\a' tone is brief and frequently heard sound, which is
   // makes it harder to notice...
   //retcode = printf("\a");
   //fflush(stdout);
   //if ( retcode < 0 )
   //{
   //   fprintf( stderr,
   //            "Failed to trigger alarm! errno: %s (%d)\n",
   //            strerror(errno), errno );

   //   return MAIN_RETCODE_FAILED_TO_ALARM;
   //}

   // ... so instead, I'll invoke a shell cmd as a convenient alternative to
   // play a .wav sound file...
   retcode = system("ffplay -nodisp -autoexit -loglevel quiet mixkit-bell-notification-933.wav");
   if ( retcode == -1 )
   {
      fprintf( stderr,
               "system(\"ffplay -nodisp -autoexit -loglevel quiet mixkit-bell-notification-933.wav\"): "
               "Child process could not be created or status could not be retrieved. errno: %s (%d)\n",
               strerror(errno), errno );

      return (int)MAIN_RETCODE_FAILED_TO_ALARM;
   }

#  ifdef DEBUG
   if ( WIFEXITED(retcode) )
   {
      printf( "system() exited /w status: %d\n", WEXITSTATUS(retcode) );
   }
#  endif

   else if ( WIFSIGNALED(retcode) )
   {
      fprintf( stderr, "system() interrupted by signal %d\n", WTERMSIG(retcode) );
      return MAIN_RETCODE_FAILED_TO_ALARM;
   }
   else if ( !WIFEXITED(retcode) )
   {
      fprintf( stderr, "system() failed somehow... Returned: %d\n", retcode );
   }

   return (int)MAIN_RETCODE_FINE;
}
