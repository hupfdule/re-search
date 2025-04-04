#!/bin/bash
savehist() {
  bash_pid_history_file=/tmp/my_bash_history_$$
  ## Save the whole history to our local history file if not yet done
  if [ ! -f "$bash_pid_history_file" ]; then
    history | sed 's/^[[:space:]]*[0-9]\+[[:space:]]\+\(.*\)/\1/g' > $bash_pid_history_file
  fi

  ## Append each executed command to the local history file
  echo $(history 1 | sed "s/^[[:space:]]*[0-9]\+[[:space:]]\+\(.*\)/\1/g") >> $bash_pid_history_file
}
export PROMPT_COMMAND='if [ "$(id -u)" -ne 0 ]; then savehist; fi'
