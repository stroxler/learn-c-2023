# Following along with Crafting Interpreters part II

This is the C bytecode VM.

I'm building it on an M1 Apple Silicon machine, the
`compile.sh` script (which I'm using in place of a makefile
in part to prevent myself from copypasta-ing makefiles I
don't understand) assumes this.

The repl loop is to run this:
```
bash compile.sh && rlwrap ./clox.exe
```
