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

# Development notes

The macros are complex enough that it's easy to start running into
errors that aren't obvious. As far as I can tell there's no way to
directly get useful error messages, but you can run
```
gcc -E problematic_file.c
```
to see the preprocessor-expanded code, which is usually enough to
figure out what the problem is. If you're still not sure, just
pipe the output of that to a file and run gcc on *it* to get
position information :)

In general I found that for segfaults running the `gcc` output
in `lldb` was pretty usable, I was able to google my way to basic
instructions on using lldb quite easily.
