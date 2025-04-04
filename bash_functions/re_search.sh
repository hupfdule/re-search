#!/bin/bash
re_search() {
  export bash_history_file="/tmp/re-search_bash_history_${USER}_${BASHPID}"
  if [ ! -e "$bash_history_file" ]; then
    savehist
  fi
  export bash_cursor_pos_file=$(mktemp -t bash.curs.XXXXXX)
  export bash_readline_cmd_file=$(mktemp -t bash.rdln.XXXXXX)
  export bash_append_char_file=$(mktemp -t bash.char.XXXXXX)
  tmp=$(mktemp -t bash-re-search.XXXXXX)
  export SEARCH_BUFFER="$READLINE_LINE"
  stty -ixon
  RE_SEARCH_SHELL=BASH re-search "$tmp"
  res="$?"
  echo "$res"
  stty -ixoff
  [ -s "$tmp" ] || return 1
  READLINE_LINE="$(cat "$tmp")"
  READLINE_POINT=${#READLINE_LINE}
  rm -f "$tmp"
  return $res
}
# Beware! This requires xdotool to work. And this will only work under X.
bind -x '"\C-r":"if re_search; then xdotool key KP_Enter; fi"'

