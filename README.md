re-search
=========

A more sophisticated history search for the [Fish Shell](https://fishshell.com).

Introduction
------------

The Fish internal search has a few flaws (at least in my opinion):

- The search string needs to be entered _before_ starting the search.
  It is not possible to alter this search string after the search has begun.

- While browsing the history, if you move the cursor [it is not possible to
  continue browsing](https://github.com/fish-shell/fish-shell/issues/2536).
  Instead you have to start browsing the history from the start.

- Browsing in the history and searching in the history are combined (both
  activated via `cursor up`. If there is already some content in the current
  commandline this will inevitably do a search in the history for that string
  instead of just browsing in it. Apart from being annoying this can also be
  [dangerous](https://github.com/fish-shell/fish-shell/issues/1668).

- Fish does not provide a way to disable its internal history search, so
  [a pure scrolling via Cursor up / down is not possible](https://github.com/fish-shell/fish-shell/issues/1671).

re-search was created to circumvent these flaws. But then it was extended to
provide a lot of quality-of-life features.


Key Features
------------

- Provide a meaningful search prompt by displaying lots of information in the search prompt:
  * the current search term
  * current search direction
  * current number of the matching commandline (repeating the search increases this number)
  * maximum number of history entries and the position of the current matching commandline in the history.
- Highlight the search term in the matching commandline
- Highlight different parts of the prompt in different colors to make them easily distinguishable and enhance readability
- Highlight in color if the current search term does not match any commandline
- Provide subsearches to combine different search strings
- Provide negative subsearches to exclude specific strings from the matching commandlines
- Provide a history browsing mode to scroll through the history without searching
- Lots of keybindings to move the cursor or execute [readline functions](https://fishshell.com/docs/current/cmds/bind.html#special-input-functions) after accepting a matching commandline


### Examples

Klick on the examples for watching them in higher quality as asciinema cast.

---

A typical search prompt. It contains the following components:

1. Subsearch terms (in square brackets). Negative ones are marked with a red exclamation mark.
2. Search direction. Green if the search was successful, otherwise red.
3. The current search term.
4. The number of the matching commandline (in parentheses). Searching further in history increases this by one.
5. The position of the current commandline in history and the amount of history entries (in square brackets).
6. The current matching commandline. The current search term is highlighted in blue and the cursor positioned at the start of the first matching string in that commandline.

[![Typical search prompt](https://github.com/user-attachments/assets/223562cb-81db-4da3-9a72-559c09aa90dc)](https://github.com/user-attachments/assets/42507268-95cd-47d4-a9e7-6e3f0437b3ac)

---

An ongoing search in re-search.

1. search for the term “ssh”
2. add this term to the subsearches
3. search for the term “debug”
4. negate that term
5. add it to the subsearches
6. search for the term “appuser”

[![Multiple search terms, 2 terms, 1 negativ term](https://github.com/user-attachments/assets/a53ff685-ca8d-4994-a59a-5631cd95045c)](https://asciinema.org/a/714283)

---

Search backward (via `Ctrl-r`) and forward (via `Ctrl-s`) in history for the given
search terms (“ssh” and “appuser”, but not containing “debug”).

As “appuser” is the current search term, this will be the highlighted one where the cursor is placed.

[![Search back and forth](https://github.com/user-attachments/assets/9994dcd5-8c3e-4199-a432-1939b95666cd)](https://asciinema.org/a/714284)

---

Accept the commandline.

In this case via `Alt-d`, the usual keybinding for the readline function
“kill-word”. The word “appuser” is removed and the cursor remains at the same
position as in the re-search prompt, ready for entering the new username
“otheruser”.

re-search supports a wide range of readline-functions to accept the
search term and directly execute the corresponding function on the resulting
commandline.

[![Accept and edit](https://github.com/user-attachments/assets/e444cc20-3d1b-489f-90fd-0b55d01dcb9b)](https://asciinema.org/a/714285)

---

Press `Ctrl-o` to accept and execute a commandline from the history and
directly jump to the next commandline in history.

This allows reexecuting a sequence of commands in the same order by hammering `Ctrl-o`.

[![Execute commands in sequence](https://github.com/user-attachments/assets/828078a8-ce7c-437b-86ac-bbf559e64c9d)](https://asciinema.org/a/714286)

---

Browse the history with `Ctrl-p` and `Ctrl-n`. This can be executed from within
re-search or via an explicit mapping from the shell (overriding the builtin
history browsing).

The prompt is shorter as no search string is involved.

[![Browse history](https://github.com/user-attachments/assets/c218ed13-0565-4c84-8f3d-5e8ca7e3e662)](https://asciinema.org/a/714287)


Installation
------------

Download one of the releases on the [relases
page](https://github.com/hupfdule/re-search/releases/).

The easiest way is to use the provided Debian package. Just install it and no
further configuration is necessary.

If re-search is to be installed on non-Debian based systems use one of the
provided tarballs instead. The tarballs come in two flavours, dynamically linked
and statically linked (packages including “-static”). Use the statically linked
version if the other one fails to resolve some libraries.

The tarballs should usually be extracted into `/usr/local`. Unfortunately fish
versions >= 4.0.0 [don’t include /usr/local/ in their search variables
anymore](https://github.com/fish-shell/fish-shell/issues/11349). This can be
fixed by either extracting the tarball to `/usr` (not recommended) or including
the following lines in a fish config file (e.g. `~/.config/fish/config.fish`):

    set -a fish_function_path /usr/local/share/fish/vendor_functions.d/
    set -a $__fish_vendor_confdirs /usr/local/share/fish/vendor_conf.d

At the moment only packages for Linux are provided. Also re-search has only been
tested on x86_64 systems.


Usage
-----

re-search is meant to be bound to a keyboard shortcut (usually `Ctrl-r` to
replace the builtin history search). To work correctly some environment
variables need to be set which allow it to prefill the search buffer, fill the
current commandline with the selected result and position the cursor correctly.

The installation packages provide some shell scripts and configuration snippets
to do this automatically. I you don’t use any of the provided packages, use them
as reference to find out which scripts needs to be placed where.


### Options

There is a single option that can be used when invoking re-search directly
(instead of via keybinding). With the option `--version` re-search prints its
version number and exits without doing a search.


### History search

After invoking re-search (by default via `Ctrl-r`) re-search will display its
prompt and wait for the search term to search for. If there is already some
content in the commandline while invoking re-search it will prefill that content
as the initial search term and start searching.

Every modification of the search term will search further for the first matching
command line.

To navigate the matching commandlines use `Ctrl-r` to search for older matches
or `Ctrl-s` to reverse the search direction and search for newer matches.

#### Multiple search terms

re-search allows for searching multiple search terms at once. For example
finding an ssh session to some host on example.com could be done by searching for
“ssh” as well as “example.com”. re-search will restrict the results to commands
that include all the given search terms. To add the current search term to the
list of search terms use `Ctrl-q`. To remove the last added search term use
`Ctrl-x`.

#### Negative search terms

re-search also allows excluding certain terms from the search. This is done by
negating the current search term. A negative search term that is added to the
list of search terms will remain negative (this is indicated by a red
exclamation mark in front of the term).

To negate the current search term use `Ctrl-z`.

#### Accepting a commandline

If the correct commandline was found it can be accepted in several ways.

##### Execute it

To accept and directly execute the commandline, press `Enter`.

To accept it, but not execute it (e.g. to modify it before executing it), press
any of the other key bindings for accepting a commandline. See [Key
bindings](#key-bindings) for all possibilities.

##### Execute it and prepare the next commandline

re-search tries to mimic Bashs `Ctrl-o` for executing an entry from the history
and immediately preparing the next one to be executed.

This makes it easy to reexecute a sequence of commands by continuously hitting
`Ctrl-o`.

An example could be: You remember you have executed the `gather_data.sh` script
in a certain directory, then edited the resulting CSV file for forwarding to
some service. The sequence of commands have been:

    cd /some/path
    ./gather_data.sh
    vi data.csv
    cp data.csv /other/path
    ./forward_data.sh

To reexecute these commands one can

1. Search for `gather_data`
2. Step back in history (to the directory switching line)
3. Press `Ctrl-o` 6 times to execute all 6 consecutive commands

Unfortunately since this functionality is baked into re-search and not provided
by Fish itself, it is only possible to execute these commands as is. It is not
possible to modify them before executing, because that would mean “accepting”
the current command and leaving re-search. So there would be no way to identify
the “next” command.


### Aborting the search

To abort the search, press `Ctrl-g`, `Ctrl-c` or `Esc`.


### Scrolling

Additionally to searching, re-search also allows scrolling through the history.
The key bindings `Ctrl-p` and `Ctrl-n` are provided for scrolling back or forth
in the history.

If a search was executed before scrolling, scrolling will start at that position
in the history.

Scrolling can also be bound to certain key strokes to override Fishs unwieldy
history scrolling, e.g. via:

    # Run re-search history scrolling with Ctrl-p
    bind \cp re_search_scroll

    # Run re-search history scrolling with “Cursor up”
    bind \e\[A re_search_scroll

This has one unfortunate side effect. The horizontal cursor position will
change after entering the re-search scrolling mode due to the differing length
of the prompts of Fish and re-search. It can be rather distracting or confusing
having to find the cursor position after the prompt change.

Your mileage may vary, so feel free try it out. But this feature is not enabled
by default and the above mentioned bindings must be placed manually into the
Fish config file.


### Prompt visualization

re-search tries to provide as much information as possible in its prompt. The
following information is present (in the following order):

- additional search terms if multiple search terms were used (in square brackets)
- search direction (in angle brackets)
- the current search term (prefixed by a red exclamation mark if negated)
- the number of the matching search term (in parentheses)
- the current and max number of history entries (in square brackets)
- the current matching commandline (the current search term will be highlighted in the commandline)

For enhanced readability colorization is used to differentiate between the
different parts of the prompt.


### Key bindings

This is the full list of recognized key bindings inside re-search.

#### Cancelling the search

Cancelling the search closes re-search and displays the shell prompt again
without modification.

* `Ctrl-c`
* `Ctrl-g`
* `Ctrl-d` (may be changed in the future to some other functionality)
* `Esc`

#### Accepting the search result

Accepting a search can be done with and without navigation / modification.

* `Ctrl-e`        Accept and place cursor at the end of the commandline
* `End`           Accept and place cursor at the end of the commandline
* `Ctrl-a`        Accept and place cursor at the start of the commandline
* `Home`          Accept and place cursor at the start of the commandline
* `Enter`         Accept and execute the result
* `Ctrl-j`        Accept and execute the result
<!-- -->
* `Ctrl-b`        Accept and execute readline `backward-char` function at the start of the search string
* `Left`          Accept and execute readline `backward-char` function at the start of the search string
* `Ctrl-f`        Accept and execute readline `forward-char` function at the start of the search string
* `Right`         Accept and execute readline `forward-char` function at the start of the search string
* `Alt-b`         Accept and execute readline `backward-word` function at the start of the search string
* `Alt-f`         Accept and execute readline `forward-word` function at the start of the search string
* `Alt-d`         Accept and execute readline `kill-word` function at the start of the search string
* `Alt-Backspace` Accept and execute readline `backward-kill-word` function at the start of the search string
* `Alt-t`         Accept and execute readline `transpose-words` function at the start of the search string
* `Alt-u`         Accept and execute readline `upcase-word` function at the start of the search string
* `Alt-l`         Accept and execute readline `downcase-word` function at the start of the search string
* `Alt-c`         Accept and execute readline `capitalize-word` function at the start of the search string
* `Ctrl-k`        Accept and execute readline `kill-line` function at the start of the search string

#### Modifying the search string

* `Ctrl-h`        Delete last character of the search string
* `Backspace`     Delete last character of the search string
* `Ctrl-w`        Delete last word of the search string
* `Ctrl-u`        Delete the whole search string

#### Miscellaneous functions

* `Ctrl-p`        Step back one entry in the history
* `Ctrl-n`        Step forward one entry in the history
* `Ctrl-r`        Search backward for the search string
* `Up`            Search backward for the search string
* `Page-Up`       Search backward for the search string
* `Ctrl-s`        Search forward for the search string
* `Down`          Search forward for the search string
* `Page-Down`     Search forward for the search string
* `Ctrl-l`        Clear screen
* `Ctrl-q`        Start subsearch
* `Ctrl-x`        Remove last subsearch string
* `Ctrl-z`        Toggle between positive and negative search
* `Ctrl-o`        Execute the result and jump to the next history entry


### Prompt customization

At the moment it is not possible to customize the prompt without recompiling
re-search. However it is rather simple to override the macro for displaying the
prompt with a custom one.

- Stop tracking changes on `config.h`

```
git update-index --assume-unchanged config.h
```

- Redefine the prompt macro in `config.h`, for example for a rather compact prompt:

```c
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

or even simpler:

```c
#define PROMPT(buffer, direction, index, result) \
	do { fprintf(stderr, "%s", result); } while (0)
```

- Recompile


Bash support
------------

While re-search was initially written for the Fish shell, it has limited support
for Bash, too. The binary itself works for both, Fish and Bash and in case the
current shell is Bash it will parse the Bash history file and search in its
entries. The functionality inside the re-search prompt is the same.

Unfortunately Bash is lacking a tool like Fishs [commandline
builtin](https://fishshell.com/docs/current/cmds/commandline.html). With Bash it
is possible to accept a search term and fill it into the current command prompt
and even position the cursor at the same spot as the last entered search term.
But all the sophisticated ways of accepting a history entry will not work. For
example, pressing `Alt-f` in the re-search prompt will accept the current
history entry, place the cursor on the start of the current search term and
execute the readline function `forward-word`. In Bash there is no possibility to
execute that readline function.

Even more disturbing is the lack of directly executing the currently displayed
history entry via `Enter`. This can be alleviated by utilizing a tool like
[xdotool](https://github.com/jordansissel/xdotool), but this is restricted to
X sessions and will not work within Wayland, a pure terminal or SSH sessions.

re-search provides shell script to use it in bash and bind it to `Ctrl-r`, but
due to the lacking support in Bash it is not deployed by default. The release
packages provide a script in the `doc/example/` directory which can be used by
sourcing it in the `.bashrc` like:

    . /usr/share/doc/re-search/examples/bash_functions/re_search.sh


History
-------

re-search was originally written by
[Julien Bonjean](https://github.com/jbonjean//re-search).
The following additions were made in this repository:

- The history file is not read from `~/.local/share/fish/fish_history`, but
  instead the `history` command is used and an event-handler function
  `re_search_savehist` is registered for writing all executed commands to
  a re-search specific temporary history file. This has the huge advantage that
  commands from other fish instances will not be visible in the current shell.  
  This is especially important when using `Ctrl-p` for stepping back in
  history as one would expect that those commands are exactly the last
  commands of the current shell.

- Support `Ctrl-o` to execute a line from the history and jump to the next
  entry. This replicates the same behavior of Bash. This is still an
  experimental feature and has a few drawbacks (for example the executed
  commandlines are not added to the current fish shell).

- Show the number of current/max history entries in the prompt.

- Allow finishing the search with a lot of readline bindings (`Ctrl-f`,
  `Ctrl-b`, `Ctrl-e`, `Ctrl-a`, `Alt-b`, `Alt-f` and some more obscure ones) and
  execute the corresponding readline function in fishs commandline. Those
  readline functions are executed from the position of the beginning of the
  search string in the command line (as does Bashs history search).

- Colorize the search string in the matching commandline and place the cursor
  at the beginning of the search string in the commandline. This makes it
  easier to spot the search string and to predict the behavior of executing
  readline functions from that position.

- Allow negative searches. Only entries that _do not_ contain the search
  string will be found then. This is especially handy with subsearches by
  filtering out unwanted results.


License
-------

re-search is distributed under the terms of the [GPL v3](LICENSE).
