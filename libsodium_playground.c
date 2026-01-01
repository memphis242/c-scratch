#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>
#include <assert.h>

#include <signal.h>

#include <sodium.h>

constexpr size_t MAX_STRING_SZ = 10'000;

static volatile sig_atomic_t bUserEndedSession = false;

static void handleSIGINT(int signum);
static bool IsNulTerminated(char * str);

int main(void)
{
   int mainrc = 0;
   int sodiumrc = 0;
   int rc = 0; // system call return code

   char * msg = nullptr;
   ptrdiff_t msgsz = 0;
   uint8_t * cipherblob = nullptr;
   ptrdiff_t cipherblobsz = 0;
   char * ciphertxt = nullptr; // base64 encoding of cipherblob
   ptrdiff_t ciphertxtsz = 0;
   char passphrase[50] = {0};
   uint8_t * key = nullptr;
   size_t keysz = 0;
   char * b64 = nullptr;
   size_t b64sz = 0;
   char * hex = nullptr;
   size_t hexsz = 0;
   uint8_t * salt = nullptr;
   size_t saltsz = 0;

   char filecounter = '0'; // FIXME: Delete this once I come up with a better scheme later...

   sodiumrc = sodium_init();
   if ( sodiumrc < 0 )
   {
      fprintf( stderr,
               "Sodium failed to initialize correctly.\n"
               "sodium_init() returned: %d. Aborting...\n",
               sodiumrc );

      return -1;
   }

   struct sigaction sa_cfg = {0};
   sa_cfg.sa_flags |= SA_RESTART; // I'd like to make sure file I/O calls are restarted if interrupted
   sigemptyset(&sa_cfg.sa_mask);
   sa_cfg.sa_handler = handleSIGINT;
   rc = sigaction( SIGINT, &sa_cfg, nullptr /* old signal cfg */ );
   if ( rc != 0 )
   {
      (void)fprintf( stderr,
               "Warning: sigaction() failed to register interrupt signal handler.\n"
               "Returned: %d, errno: %s (%d): %s\n"
               "You won't be able to stop the program gracefully /w Ctrl+C, although \n"
               "Ctrl+C will still terminate the program.\n",
               rc, strerrorname_np(errno), errno, strerror(errno) );

      mainrc |= 0x01;
   }

   rc = printf("Welcome! Let's play /w libsodium!😃\n");
   if ( rc < ( (int)sizeof("Welcome! Let's play /w libsodium!😃\n") - 1 ) )
   {
      // Can't print, so we'll just exit /w a specific return code to alert user
      return -2;
   }

   constexpr size_t WHILE_LOOP_CAP = 1'000'000;
   size_t nreps = 0;
   while ( !bUserEndedSession && nreps++ < WHILE_LOOP_CAP )
   {
      char cmd[64];

      (void)printf("\n> ");
      (void)fflush(stdout);

      if ( fgets(cmd, sizeof cmd, stdin) == nullptr )
      {
         // EOF encountered or I/O error encountered... Either way, break free.
         printf("\nExiting...\n");
         break;
      }

      // Replace newline /w null-termination
      char * newlineptr = memchr(cmd, '\n', sizeof cmd);
      if ( newlineptr == nullptr )
      {
         int c;
         while ( (c = fgetc(stdin)) != '\n' && c != EOF );

         fprintf( stderr,
                  "Error: Too many characters entered. Please try again.\n" );
         continue;
      }
      assert ( newlineptr < (cmd + sizeof(cmd)) );
      *newlineptr = '\0';

      // Treat cmd as case-insensitive
      for ( char * ptr = cmd; ptr != nullptr && ptr < (cmd + sizeof(cmd)) && *ptr != '\0'; ++ptr )
         *ptr = (char)tolower(*ptr);

      // Parse cmd
      if ( strcmp(cmd, "newmsg") == 0 )
      {
         char buf[2048];

         (void)printf("Enter message: ");
         (void)fflush(stdout);

         if ( fgets(buf, sizeof buf, stdin) == nullptr )
         {
            // EOF encountered or I/O error encountered... Either way, break free.
            (void)printf("\nExiting...\n");
            break;
         }

         newlineptr = memchr(buf, '\n', sizeof buf);
         if ( newlineptr == nullptr )
         {
            int c;
            while ( (c = fgetc(stdin)) != '\n' && c != EOF );

            (void)fprintf(stderr, "Error: Too many characters entered.\n");
            continue;
         }
         assert( newlineptr < (buf + sizeof(buf)) );
         *newlineptr = '\0';

         // Copy msg over to persistent space outside of this scope
         msgsz = newlineptr - buf + 1;
         free(msg); // Override previous msg
         msg = malloc( (size_t)(msgsz) * sizeof(char) );
         if ( msg == nullptr )
         {
            (void)fprintf( stderr, "Error: Couldn't malloc() a buffer for msg\n" );
            continue;
         }
         (void)memcpy( msg, buf, (size_t)msgsz );

         (void)printf("Successfully received msg.\n");
      }

      else if ( strcmp(cmd, "printmsg") == 0 )
      {
         if ( msg == nullptr )
         {
            (void)fprintf(stderr, "No msg present. Aborting cmd...\n");
            continue;
         }

         assert(IsNulTerminated(msg));

         (void)printf("%s\n", msg);
      }

      else if ( strcmp(cmd, "addpassphrase") == 0 )
      {
         (void)printf("Note old encrypted content will remain!\n"
                      "New Passphrase: ");

         if ( fgets(passphrase, sizeof passphrase, stdin) == nullptr )
         {
            // EOF encountered or I/O error encountered... Either way, exit cmd.
            // Clear EOF indicator in stream so top level doesn't also exit
            clearerr(stdin);
            (void)printf("\nExiting cmd...\n");
            break;
         }

         newlineptr = memchr(passphrase, '\n', sizeof passphrase);
         if ( newlineptr == nullptr )
         {
            int c;
            while ( (c = fgetc(stdin)) != '\n' && c != EOF );

            (void)fprintf(stderr, "Error: Too many characters entered.\n");
            continue;
         }
         assert( newlineptr < (passphrase + sizeof(passphrase)) );
         *newlineptr = '\0';
         assert( IsNulTerminated(passphrase) );

         (void)printf("Successfully updated passphrase\n");

         // TODO: Provide choice for KDF + AEAD encryption algorithm + parameters...

         (void)printf("Generating new encryption key using default libsodium KDF"
                      " for XChaCha20Poly1305_IETF AEAD cipher...\n");

         // We will override previous stores
         free(salt);
         free(key);

         saltsz = crypto_pwhash_SALTBYTES;
         salt = malloc(saltsz);
         keysz = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
         key = malloc(keysz);

         randombytes_buf(salt, saltsz);
         sodiumrc = crypto_pwhash( key, keysz,
                                   passphrase, strnlen(passphrase, sizeof passphrase),
                                   salt,
                                   crypto_pwhash_OPSLIMIT_SENSITIVE,
                                   crypto_pwhash_MEMLIMIT_SENSITIVE,
                                   crypto_pwhash_ALG_DEFAULT );
         if ( sodiumrc != 0 )
         {
            (void)fprintf( stderr,
                     "Failed to create encryption key from KDF.\n"
                     "crypto_pwhash returned: %d\n",
                     sodiumrc );
            continue;
         }

         (void)printf("Successfully generated new encryption key.\n");

         // Save the hex encoding for later printing
         hexsz= keysz * 2 + 1;
         hex = malloc( hexsz * sizeof(char) );
         (void)sodium_bin2hex( hex, hexsz,
                               key, keysz );

         // Save the base64 encoding for later printing
         constexpr int b64variant = sodium_base64_VARIANT_ORIGINAL;
         b64sz= sodium_base64_ENCODED_LEN(keysz, b64variant);
         b64 = malloc( b64sz * sizeof(char) );
         (void)sodium_bin2base64( b64, b64sz,
                                  key, keysz,
                                  b64variant );

         // Write the raw binary key to a file for later viewing
         char testkey_filename[ sizeof("./testkey") + 1 + sizeof(".bin") ] = {0};
         (void)strcat(testkey_filename, "./testkey");
         (void)strcat(testkey_filename, (char[]){filecounter, '\0'});
         (void)strcat(testkey_filename, ".bin");
         assert(IsNulTerminated(testkey_filename));

         FILE * fd = fopen(testkey_filename, "wb");
         if ( fd == nullptr )
         {
            fprintf( stderr,
               "Error: Failed to open file %s\n"
               "fopen() returned nullptr, errno: %s (%d): %s\n",
               testkey_filename,
               strerrorname_np(errno), errno, strerror(errno) );

            continue;
         }

         {
            size_t nwritten = fwrite( key,
                                      1 /* element sz */,
                                      keysz /* n items */,
                                      fd );
            if ( nwritten != keysz )
            {
               fprintf( stderr,
                  "Error: Failed to write all the bytes of the key to %s\n"
                  "Wrote only %zu bytes out of %zu (%zu bytes short)\n"
                  "errno: %s (%d): %s\n",
                  testkey_filename,
                  nwritten, keysz, keysz - nwritten,
                  strerrorname_np(errno), errno, strerror(errno) );
            }
         }

         rc = fclose(fd);
         if ( rc != 0 )
         {
            // TODO: Consider checking if errno was EINTR?
            fprintf( stderr,
               "Error: Failed to open file %s\n"
               "fopen() returned nullptr, errno: %s (%d): %s\n",
               testkey_filename,
               strerrorname_np(errno), errno, strerror(errno) );

            continue;
         }

         // Repeat for text encodings
         assert(IsNulTerminated(testkey_filename));
         testkey_filename[ strlen(testkey_filename) - sizeof("bin") + 1 ] = '\0';
         (void)strcat(testkey_filename, "hex");

         fd = fopen(testkey_filename, "w");
         if ( fd == nullptr )
         {
            fprintf( stderr,
               "Error: Failed to open file %s\n"
               "fopen() returned nullptr, errno: %s (%d): %s\n",
               testkey_filename,
               strerrorname_np(errno), errno, strerror(errno) );

            continue;
         }

         {
            int nwritten = fprintf( fd, "%s", hex );
            if ( nwritten != ((int)hexsz - 1) )
            {
               fprintf( stderr,
                  "Error: Failed to write all the bytes of the hex encoding to %s\n"
                  "Wrote only %d bytes out of %zu (%d bytes short)\n"
                  "errno: %s (%d): %s\n",
                  testkey_filename,
                  nwritten, hexsz, (int)hexsz - nwritten,
                  strerrorname_np(errno), errno, strerror(errno) );
            }
         }

         rc = fclose(fd);
         if ( rc != 0 )
         {
            // TODO: Consider checking if errno was EINTR?
            fprintf( stderr,
               "Error: Failed to open file %s\n"
               "fopen() returned nullptr, errno: %s (%d): %s\n",
               testkey_filename,
               strerrorname_np(errno), errno, strerror(errno) );

            continue;
         }

         testkey_filename[ strlen(testkey_filename) - sizeof("hex") + 1 ] = '\0';
         (void)strcat(testkey_filename, "b64");

         fd = fopen(testkey_filename, "w");
         if ( fd == nullptr )
         {
            fprintf( stderr,
               "Error: Failed to open file %s\n"
               "fopen() returned nullptr, errno: %s (%d): %s\n",
               testkey_filename,
               strerrorname_np(errno), errno, strerror(errno) );

            continue;
         }

         {
            int nwritten = fprintf( fd, "%s", b64 );
            if ( nwritten != ((int)b64sz - 1) )
            {
               fprintf( stderr,
                  "Error: Failed to write all the bytes of the base64 encoding to %s\n"
                  "Wrote only %d bytes out of %zu (%d bytes short)\n"
                  "errno: %s (%d): %s\n",
                  testkey_filename,
                  nwritten, b64sz, (int)b64sz - nwritten,
                  strerrorname_np(errno), errno, strerror(errno) );
            }
         }

         rc = fclose(fd);
         if ( rc != 0 )
         {
            // TODO: Consider checking if errno was EINTR?
            fprintf( stderr,
               "Error: Failed to open file %s\n"
               "fopen() returned nullptr, errno: %s (%d): %s\n",
               testkey_filename,
               strerrorname_np(errno), errno, strerror(errno) );

            continue;
         }

         filecounter++;
      }

      else if ( strcmp(cmd, "printkey") == 0 )
      {
         assert( (key != nullptr && keysz > 0)
                 || (key == nullptr && keysz == 0) );
         assert( (hex != nullptr && hexsz > 0)
                 || (hex == nullptr && hexsz == 0) );

         if ( key == nullptr )
         {
            (void)fprintf(stderr, "No key present. Aborting cmd...\n");
            continue;
         }

         if ( hex == nullptr )
         {
            (void)fprintf(stderr, "No hex encoding present, skipping.\n");
         }
         else
         {
            assert(IsNulTerminated(hex));

            (void)printf("Hex: %s\n", hex);
         }

         if ( b64 == nullptr )
         {
            (void)fprintf(stderr, "No base64 encoding present, skipping.\n");
         }
         else
         {
            assert(IsNulTerminated(hex));

            (void)printf("Base64: %s\n", b64);
         }
      }

      else if ( strcmp(cmd, "encryptmsg") == 0 )
      {
         // TODO
         (void)printf("Not implemented yet.\n");
      }

      else if ( strcmp(cmd, "decryptciphertxt") == 0 )
      {
         // TODO
         (void)printf("Not implemented yet.\n");
      }

      else if ( strcmp(cmd, "printciphertxt") == 0 )
      {
         if ( ciphertxt == nullptr )
         {
            (void)fprintf(stderr, "Error: No cipher text present.\n");
            continue;
         }

         if ( !IsNulTerminated(ciphertxt) )
         {
            (void)fprintf(stderr, "ciphertxt is not terminated. Aborting cmd...\n");
            continue;
         }
         (void)printf("%s\n", ciphertxt);
      }

      else if ( strcmp(cmd, "storemsg") == 0 )
      {
         // TODO
         (void)printf("Not implemented yet.\n");
      }

      else if ( strcmp(cmd, "storeciphertxt") == 0 )
      {
         // TODO
         (void)printf("Not implemented yet.\n");
      }

      else if ( strcmp(cmd, "storecipherblob") == 0 )
      {
         // TODO
         (void)printf("Not implemented yet.\n");
      }

      else if ( strcmp(cmd, "load") == 0 )
      {
         // TODO
         (void)printf("Not implemented yet.\n");
      }

      else if ( strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0 )
      {
         bUserEndedSession = true;
         break;
      }

      else
      {
         (void)fprintf(stderr, "Invalid command: %s. Please try again.\n", cmd);
         continue;
      }
      
   }

   assert( nreps < WHILE_LOOP_CAP );

   // TODO: Graceful shutdown
   // Free any dynamically allocated buffers...
   free(msg);
   free(cipherblob);
   free(ciphertxt);
   free(key);
   free(salt);
   free(b64);
   free(hex);
   // Close any files that we opened...
   // TODO

   return mainrc;
}

static void handleSIGINT(int signum)
{
   (void)signum;

   bUserEndedSession = true;
}

static bool IsNulTerminated(char * str)
{
   for ( size_t i = 0 ; i < MAX_STRING_SZ; ++i )
      if ( str[i] == '\0' )
         return true;

   return false;
}
