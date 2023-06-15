# lldb

This exercise doesn't have new code, it's just a debugger intro.

To start our tiny program inside lldb, run:
```
make program
lldb ./program
```

To set a breakpoint, you can use either
```
breakpoint set program.c:main
```
or
```
breakpoint set --file program.c --line 8
```

Then, you can launch execution with `run`.

Some handy functions to use inside a breakpoint:
```
bt
p <expr>
continue
```
