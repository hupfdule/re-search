function re_search
	set -x fish_history_file (mktemp -t fish.hist.XXXXXX)
	set -x fish_cursor_pos_file (mktemp -t fish.curs.XXXXXX)
	set -x fish_readline_cmd_file (mktemp -t fish.rdln.XXXXXX)
	history --null --reverse -n (echo "1024 * 256" | bc) > "$fish_history_file"
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
	if [ $res = 0 ]
		commandline -f execute
	end
	rm -f $tmp $fish_history_file
end
