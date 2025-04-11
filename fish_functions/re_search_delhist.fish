function re_search_delhist --on-event fish_exit
	set -x fish_pid_history_file /tmp/re-search/fish_history_"$USER"_"$fish_pid"
	if [ -f $fish_pid_history_file ]
		rm -f $fish_pid_history_file
	end
end

# vim: tabstop=2 shiftwidth=2 noexpandtab :
