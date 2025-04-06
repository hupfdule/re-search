delhist() {
  export bash_history_file="/tmp/re-search_bash_history_${USER}_${BASHPID}"
  trap "rm -rf ${bash_history_file}" EXIT
}
