#!/usr/bin/env bash

# Compile clox by hand on a macos machine

# Notes:
# - The -c flag on gcc causes it to ouptut an object file
# - The -L and -lSystem flags are required on macos to find
#   the system library.

export CFLAGS="-c"

gcc -c -o main.o main.c
gcc -c -o chunk.o chunk.c

ld \
   -macos_version_min 13.3.1 -arch arm64 \
   -L$(xcode-select -p)/SDKs/MacOSX.sdk/usr/lib -lSystem \
   -o clox.exe \
   main.o
