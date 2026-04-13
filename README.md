# festum

**The smallest fully interactive shell you have ever seen — a thousand lines of C99, zero dependencies, zero libraries, zero configuration files — with tab completion, line editing, command history, pipes, redirections, and every feature you actually use in a shell, built entirely from raw system calls and nothing else.**

## Why Another Shell

Every shell in common use today is an archaeological dig. Bash is forty years of backward-compatible accretion, hundreds of thousands of lines of C serving a language that nobody chose and everybody tolerates. Zsh is a quarter-million lines of C++ with a plugin ecosystem, a theme engine, and a community that spends more time configuring their prompt than writing software. Fish reimagined the shell and then reimagined it again in Rust, because apparently the first reimagining wasn't enough abstraction. Even dash, the so-called minimal shell, links against libedit or readline for interactive use, because the assumption has always been that reading a line of input from a terminal is someone else's problem.

Festum rejects that assumption entirely. Reading input from a terminal is not a hard problem. It is a solved problem. It requires exactly one POSIX header — `termios.h` — and a few hundred lines of straightforward byte-at-a-time processing. The entire interactive editing system, the tab completion engine, the history navigator, the cursor manager — all of it fits in the same single file as the command parser, the pipe builder, and the process executor. One file. One compilation unit. One compiler invocation. No libraries. No dependencies. No configuration.

## Tab Completion

Other shells delegate completion to readline or libedit — libraries that arrive with their own configuration files, their own keybinding formats, their own initialization sequences, their own abstraction layers over abstraction layers. Festum handles completion itself, in roughly two hundred lines of C, by doing the only two things that tab completion actually needs to do: searching directories and comparing strings.

Press Tab after a partial command name and festum walks every directory in `$PATH`, matching candidates and eliminating duplicates, faster than readline has finished parsing its `.inputrc`. Press Tab after a partial filename and festum opens the relevant directory, reads its entries, filters by prefix, and either completes immediately or shows the full candidate list. A single match inserts the completion with a trailing space — or a trailing slash for directories, because that is what you expect and festum does what you expect. Multiple matches fill the longest common prefix on the first Tab and display all candidates on the second. This is the exact behavior of every major shell's completion system, implemented without a single external function call.

## Line Editing

The entire interactive experience — cursor movement with arrow keys, Home, End, Delete, Ctrl-A to jump to the beginning, Ctrl-E to jump to the end, Ctrl-K to kill to the end of the line, Ctrl-U to kill to the beginning, Ctrl-W to delete the previous word, Ctrl-L to clear the screen — is implemented from scratch using ANSI escape sequences and POSIX termios. No ncurses. No termcap. No terminfo database lookups. No abstraction of any kind between the shell and the terminal.

Every keystroke arrives as a raw byte. Escape sequences are parsed inline. The screen is redrawn with direct cursor positioning. Insert-anywhere editing works by shifting the buffer and repainting from the cursor position forward. This is not a simplified or degraded editing experience — it is the same editing model that readline provides, implemented in a fraction of the code, without the configuration overhead, without the library dependency, without the `.inputrc` that nobody remembers the syntax of.

## Command History

Arrow up recalls the previous command. Arrow down moves forward. The current input is preserved when you begin browsing history and restored when you return. Two hundred and fifty-six entries deep, with duplicate suppression, without writing a single byte to disk. Simple, fast, correct — exactly the history behavior you rely on in every interactive session, without the complexity of `.bash_history` management, `HISTSIZE` tuning, or timestamp decoration.

## Pipes, Redirections, And Everything Else

Festum handles arbitrary-depth pipelines, input and output redirection with `>`, `>>`, and `<`, background execution with `&`, environment variable expansion for `$VAR`, `${VAR}`, `$?`, and `$$`, single and double quoting with backslash escapes, tilde expansion, comment lines, and the six builtins that must execute in the shell process itself — `cd`, `exit`, `export`, `unset`, `echo`, and `pwd`. It executes scripts non-interactively when given a filename argument. It reaps background children without blocking. It handles SIGINT gracefully in both the shell process and its children.

This is everything a shell needs to do. Not everything a shell can be made to do — everything it needs to do.

## Building

```bash
make
```

Or directly:

```bash
cc -O2 -o festum festum.c
```

One file. One command. Zero dependencies. A C99 compiler and a POSIX system.

## Usage

```bash
./festum
```

```bash
./festum script.sh
```

## License

Free. Public domain. Use however you like.
