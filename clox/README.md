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

The from-source loop is to run this:
```
bash compile.sh && ./clox.exe try.lox
```

# Development notes

The macros are complex enough that it's easy to start running into errors that
aren't obvious. As far as I can tell there's no way to directly get useful
error messages, but you can run
```
gcc -E problematic_file.c
```
to see the preprocessor-expanded code, which is usually enough to figure out
what the problem is. If you're still not sure, just pipe the output of that to
a file and run gcc on *it* to get position information :)

In general I found that for segfaults running the `gcc` output in `lldb` was
pretty usable, I was able to google my way to basic instructions on using lldb
quite easily. I was able to work out the source of several segfaults pretty
quickly this way (usually in that case you can get pretty far from just
figuring out the current line of code).

# Precedence and Pratt Parsing

Pratt parsing is a little weird to get used to, I tried to take some inline
notes. The important thing is that the algorithm is only really related
to the resolving of binary and unary operators where the grammar would be
ambiguous without precedence, but it's used *everywhere*. The non-operator
forms, including both explicitly-delimited forms like `grouping` and atomic
forms like `number` all have prefix rules with PREC_NONE.

I keep finding that "precedence" turns me around a bit. High precedence means
tight-binding, which actually means a "low level" operation whose span in an
expression is small. Low precedence is vice/versa, a "high-level" operation
with a big span. The goal of precedence limits in Pratt parsing is to break
out of tight-binding operations and unwind the stack to where we are parsing
some looser-binding operation whenever necessary.

Precedence is always set to PREC_NONE for explicitly delimited forms like
groupings and calls, because the explicitness means that by design you can nest
*any* expression. The lowest level actual operator is bang, which also has
PREC_NONE.

One thing that's a little funny about this Pratt parser is that we set
precedence explicitly without checking the parent precedence, which causes an
expression like `-!8+7` to compile okay, with the `8 + 7` nested *inside* of
the `!` which is recursively inside the unary `-`.

In many language this would be a syntax error (although C allows it).  To make
it error here we would need to track enough state to know when we've hit nested
unary operations where the precedence decreased. That would be more work, and
our behavior is well-defined even if it's a bit strange so we don't worry about
it here.
