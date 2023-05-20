#!/usr/bin/env bash

# Compile clox by hand on a macos machine

# Notes:
# - The -c flag on gcc causes it to ouptut an object file
# - The -L and -lSystem flags are required on macos to find
#   the system library.

export CFLAGS="-c"

gcc -c -o memory.o memory.c
gcc -c -o value.o value.c
gcc -c -o chunk.o chunk.c
gcc -c -o vm.o vm.c
gcc -c -o debug.o debug.c
gcc -c -o scanner.o scanner.c
gcc -c -o compiler.o compiler.c
gcc -c -o main.o main.c

ld \
	-macos_version_min 13.3.1 -arch arm64 \
	-L$(xcode-select -p)/SDKs/MacOSX.sdk/usr/lib -lSystem \
	-o clox.exe \
	main.o memory.o value.o chunk.o vm.o \
	scanner.o compiler.o debug.o
