#!/bin/bash
gcc \
   -std=c23 \
   -Og -g3 \
   -Wall -Wextra \
   -Wpedantic -pedantic-errors \
   -D_POSIX_C_SOURCE=200809L \
   -o alarmtimer \
   alarmtimer.c
