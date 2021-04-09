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
* Copy the file `re_search.fish` to the directory `~/.config/fish/functions/`.
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
* [C-q]
* couchbase

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

### Internal key bindings

* C-r, up, pg-up: backward search.
* C-s, down, pg-down: forward search.
* C-c, C-g, left, esc, home: cancel search.
* C-e, right, end: accept result.
* C-l: clear screen
* C-o: execute current entry and jump the next one in history.
* C-q: save search result and start sub-search.
* C-u: delete the current search term
* C-w: delete a word backwards
* C-p: cycle through history backwards
* C-n: cycle through history forwards
* Enter: execute result.

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
  instead the `history` command is used. This has the huge advantage that
  command from other fish instances will not be visible in the current
  shell.  
  This is especially important when using `Ctrl-p` for stepping back in
  history as one would expected that those commands are exactly the last
  commands of the current shell.  
  Unfortunately this leads to a noticeable delay on starting re-search as
  executing the `history` command takes quiet some time.

- Support Ctrl-o to execute a line from the history and jump to the next
  entry. This replicates the same behavior of Bash. This is still an
  experimental feature and has a few drawbacks (for example the executed
  commandlines are not added to the current fish shell).

- Show the number of current/max history entries in the prompt.

- Allow finishing the search with a lot of readline bindings (Ctrl-f,
  Ctrl-e, Ctrl-a, Alt-b, Alt-f and some more obscure ones) and execute the
  corresponding readline function in fishs commandline. Those readline
  functions are executed from the position of the beginning of the search
  string in the command line (as does Bashs history search).

- Colorize the search string the found commandline and place the cursor at
  the beginning of the search string in the commandline. This makes it
  easier to spot the search string and to predict the behavior of executing
  readline functions from that position.
