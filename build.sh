# gcc -Wall -Wextra -Wpedantic -pedantic-errors -fsanitize=undefined -fsanitize-trap -fsanitize=enum -fsanitize=bool -fsanitize=bounds -fsanitize=address -fanalyzer -std=c23 -D_POSIX_C_SOURCE=200809L -Og -g3 -o sp-client sp-client.c

gcc -Wall -Wextra -Wpedantic -pedantic-errors -fsanitize=undefined -fsanitize-trap -fsanitize=enum -fsanitize=bool -fsanitize=bounds -fsanitize=address -fanalyzer -std=c23 -D_POSIX_C_SOURCE=200809L -Og -g3 -o alarm_timer alarm_timer.c
