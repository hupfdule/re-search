re-search
=========

Basic incremental history search, implemented to be used with fish shell.

It doesn't support all the terminal implementations because of some ANSI
escape sequences used, and is only tested on GNU/Linux.

This is a fork of
[jbonjean/re-search](https://github.com/jbonjean/re-search) with a bunch of
extensions. See [Changes to upstream](#changes-to-upstream) for a list of changes.

### Install

* Compile and add the binary to your PATH.
* Copy the files `re_search.fish` and `savehist.fish` to the directory `~/.config/fish/functions/`.
* Add the binding to `~/.config/fish/functions/fish_user_key_bindings.fish`:
```
bind \cr re_search
```

#### Duplicate history entries

Because of performance issues with large history file, the duplicate check is
disabled by default. If you want to enable it, just pass the correct cflag to
make:

```
make CFLAGS=-DCHECK_DUPLICATES
```

#### Sub-search (experimental)

This feature allows you to do a new search in the current results.

For example, you are looking for the git-clone command of couchbase repository,
the following sequence shows how it could help you:

* [C-r] clone
* [C-q] couchbase

#### Enable bash-like history scrolling

By default fish does not scroll the history buffer on "up" an "C-p", but
instead executes a search with the current commandline content as search
string.

To enable the usual scrolling regardless of the current content of the
commandline copy the file `re_search_scroll.fish` to the directory
`~/.config/fish/functions/` and add a binding to
`~/.config/fish/functions/fish_user_key_bindings.fish`:
```
bind \cp re_search_scroll
```
to bind it to "Ctrl-p" or
```
bind \e\[A re_search_scroll
```
to bind it to the "Up" arrow.

### Key bindings

#### Cancelling the search

Cancelling the search closes re-search in displays the shell prompt again
without modification.

* `Ctrl-c`
* `Ctrl-g`
* `Ctrl-d` (maybe changed in a the future to some other functionality)
* `Left`
* `Esc`

#### Accepting the search result

Accepting a search can be done with and without navigation / modification.

* `Right` Accept and place cursor at the end of the commandline
* `Ctrl-e` Accept and place cursor at the end of the commandline
* `Ctrl-a` Accept and place cursor at the start of the commandline
* `Enter` Accept and execute the result
* `Ctrl-j` Accept and execute the result
* `Ctrl-f` Accept and place cursor at the start of the search string

* `Ctrl-b` Accept and execute readline `backward-char` function at the start of the search string
* `Alt-b` Accept and execute readline `backward-word` function at the start of the search string
* `Alt-f` Accept and execute readline `forward-word` function at the start of the search string
* `Alt-d` Accept and execute readline `kill-word` function at the start of the search string
* `Alt-Backspace` Accept and execute readline `backward-kill-word` function at the start of the search string
* `Alt-t` Accept and execute readline `transpose-words` function at the start of the search string
* `Alt-u` Accept and execute readline `upcase-word` function at the start of the search string
* `Alt-l` Accept and execute readline `downcase-word` function at the start of the search string
* `Alt-c` Accept and execute readline `capitalize-word` function at the start of the search string
* `Ctrl-k` Accept and execute readline `kill-line` function at the start of the search string

#### Modifying the search string

* `Ctrl-h` Delete last character of the search string
* `Backspace` Delete last character of the search string
* `Ctrl-w` Delete last word of the search string
* `Ctrl-u` Delete the whole search string

#### Miscellaneous functions

* `Ctrl-p` Step back one entry in the history
* `Ctrl-n` Step forward one entry in the history
* `Ctrl-r` Search backward for the search string
* `Up` Search backward for the search string
* `Page-Up` Search backward for the search string
* `Ctrl-s` Search forward for the search string
* `Down` Search forward for the search string
* `Page-Down` Search forward for the search string
* `Ctrl-l` Clear screen
* `Ctrl-q` Start subsearch
* `Ctrl-o` Execute the result and jump to the next history entry

### Customize the prompt

* Stop tracking changes on `config.h`
```
git update-index --assume-unchanged config.h
```
* Redefine the prompt macro in `config.h`, for example, a very simple prompt
could look like:
```
#define PROMPT(buffer, direction, index, result) \
        do { fprintf(stderr, "%s", result); } while (0)
```
or, a more compact version of the native prompt:
```
#define PROMPT(buffer, saved, direction, index, result) \
        do { \
        	fprintf(stderr, "[%c%d] %s", direction[0], index, buffer); \
        	if (index > 0) {\
	        	int i = 0; \
        		i = fprintf(stderr, " > %s", result); \
        		fprintf(stderr, "\033[%dD", i); \
        	} \
        } while (0)
 ```
* Recompile

### Bash support (experimental)

* Switch to Bash support with the compilation flag:
```
make CFLAGS=-DBASH
```
* If it's not already configured, it is better to make bash save commands to
  the history file in realtime:
```
shopt -s histappend
PROMPT_COMMAND="history -a;$PROMPT_COMMAND"
```
* Also, because ctrl-s is used for forward search, you may need to disable flow control:
```
stty -ixon
```
#### Add the binding:

```
re_search() {
	SEARCH_BUFFER="$READLINE_LINE" re-search > /tmp/.re-search
	res="$?"
	[ -s /tmp/.re-search ] || return 1
	READLINE_LINE="$(cat /tmp/.re-search)"
	READLINE_POINT=${#READLINE_LINE}
	return $res
}
bind -x '"\C-r":"if re_search; then xdotool key KP_Enter; fi"'
```

### Changes to upstream

- The history file is not read from `~/.local/share/fish/fish_history`, but
  instead the `history` command is used and an event-handler function
  `savehist` is registered for writing all executed commands to a re-search
  specific temporary history file. This has the huge advantage that
  commands from other fish instances will not be visible in the current
  shell.  
  This is especially important when using `Ctrl-p` for stepping back in
  history as one would expect that those commands are exactly the last
  commands of the current shell.  

- Support Ctrl-o to execute a line from the history and jump to the next
  entry. This replicates the same behavior of Bash. This is still an
  experimental feature and has a few drawbacks (for example the executed
  commandlines are not added to the current fish shell).

- Show the number of current/max history entries in the prompt.

- Allow finishing the search with a lot of readline bindings (Ctrl-f,
  Ctrl-b, Ctrl-e, Ctrl-a, Alt-b, Alt-f and some more obscure ones) and
  execute the corresponding readline function in fishs commandline. Those
  readline functions are executed from the position of the beginning of the
  search string in the command line (as does Bashs history search).

- Colorize the search string in the found commandline and place the cursor
  at the beginning of the search string in the commandline. This makes it
  easier to spot the search string and to predict the behavior of executing
  readline functions from that position.
