function re_search_savehist --on-event fish_postexec
	set -x fish_pid_history_file /tmp/re-search_fish_history_"$USER"_"$fish_pid"
	if not [ -f $fish_pid_history_file ]
		history --null --reverse > $fish_pid_history_file
	end

	if not [ -z $argv ]
		echo -n -E $argv >> $fish_pid_history_file
		echo -n -e '\0'  >> $fish_pid_history_file
	end
end

# vim: tabstop=2 shiftwidth=2 noexpandtab :
