#!/bin/bash
re_search_savehist() {
  bash_history_file="/tmp/re-search_bash_history_${USER}_${BASHPID}"
  ## Delete the history file on shell exit
  trap "rm -rf ${bash_history_file}" EXIT

  ## Save the whole history to our local history file if not yet done
  if [ ! -f "$bash_history_file" ]; then
    history | sed 's/^[[:space:]]*[0-9]\+[[:space:]]\+\(.*\)/\1/g' > $bash_history_file
  fi

  ## Append each executed command to the local history file
  echo $(history 1 | sed "s/^[[:space:]]*[0-9]\+[[:space:]]\+\(.*\)/\1/g") >> $bash_history_file
}
## save history every time a prompt is renewed (== after each command execution)
export PROMPT_COMMAND='if [ "$(id -u)" -ne 0 ]; then re_search_savehist; fi'

re_search() {
  export bash_history_file="/tmp/re-search_bash_history_${USER}_${BASHPID}"
  if [ ! -e "$bash_history_file" ]; then
    re_search_savehist
  fi
  export re_search_cursor_pos_file=$(mktemp -t re_search_curs.XXXXXX)
  export re_search_readline_cmd_file=$(mktemp -t re_search_rdln.XXXXXX)
  export re_search_append_char_file=$(mktemp -t re_search_char.XXXXXX)
  tmp=$(mktemp -t bash-re-search.XXXXXX)
  export SEARCH_BUFFER="$READLINE_LINE"
  stty -ixon
  stty susp undef # allow us to use ctrl-z in re-search
  RE_SEARCH_SHELL=BASH re-search "$tmp"
  res="$?"
  stty -ixoff
  [ -s "$tmp" ] || return 1
  READLINE_LINE="$(cat "$tmp")"
  READLINE_POINT=${#READLINE_LINE}
  ## Here we could execute the readline command in $re_search_readline_cmd_file and
  ## append the character specified in $re_search_append_char_file.
  ## But it seems Bash doesn’t support this use case…
  rm -f "$tmp" "$re_search_cursor_pos_file" "$re_search_readline_cmd_file" "$re_search_append_char_file"
  return $res
}

## Beware! This requires xdotool to work. And this will only work under X.
bind -x '"\C-r":"if re_search; then xdotool key KP_Enter; fi"'

