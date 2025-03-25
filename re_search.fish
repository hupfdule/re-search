function re_search
	if not [ -e /tmp/fish-re-search/my_fish_history_$fish_pid ]
		savehist
	end
	set -x fish_history_file /tmp/fish-re-search/my_fish_history_$fish_pid
	set -x fish_cursor_pos_file (mktemp -t fish.curs.XXXXXX)
	set -x fish_readline_cmd_file (mktemp -t fish.rdln.XXXXXX)
	set -x fish_append_char_file (mktemp -t fish.char.XXXXXX)
	set -l tmp (mktemp -t fish.XXXXXX)
	set -x SEARCH_BUFFER (commandline -b)
	re-search $tmp
	set -l res $status
	commandline -f repaint
	if [ -s $tmp ]
		commandline -r (cat $tmp)
	end
	if [ -s $fish_cursor_pos_file ]
		commandline -C (cat $fish_cursor_pos_file)
	end
	if [ -s $fish_readline_cmd_file ]
		commandline -f (cat $fish_readline_cmd_file)
	end
	if [ -s $fish_append_char_file ]
		commandline -i (cat $fish_append_char_file)
	end
	if [ $res = 0 ]
		commandline -f execute
	end
	rm -f $tmp $fish_cursor_pos_file $fish_readline_cmd_file $fish_append_char_file
end

# vim: tabstop=2 shiftwidth=2 noexpandtab :
