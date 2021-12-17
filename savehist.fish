function savehist --on-event fish_postexec
	set -x fish_pid_history_file /tmp/my_fish_history_$fish_pid
	if not [ -f $fish_pid_history_file ]
		history --null --reverse > $fish_pid_history_file
	end

	if not [ -z $argv ]
		printf $argv'\0' >> $fish_pid_history_file
	end
end
