function re_search
	set -x fish_history_file /tmp/re-search/fish_history_"$USER"_"$fish_pid"
	if not [ -e $fish_history_file ]
		re_search_savehist
	end
	set -x re_search_cursor_pos_file (mktemp -t /tmp/re-search/re_search_curs.XXXXXX)
	set -x re_search_readline_cmd_file (mktemp -t /tmp/re-search/re_search_rdln.XXXXXX)
	set -x re_search_append_char_file (mktemp -t /tmp/re-search/re_search_char.XXXXXX)
	set -l tmp (mktemp -t /tmp/re-search/fish.re-search.XXXXXX)
	set -x SEARCH_BUFFER (commandline -b)
	re-search $tmp
	set -l res $status
	commandline -f repaint
	if [ -s $tmp ]
		commandline -r (cat $tmp)
	end
	if [ -s $re_search_cursor_pos_file ]
		commandline -C (cat $re_search_cursor_pos_file)
	end
	if [ -s $re_search_readline_cmd_file ]
		commandline -f (cat $re_search_readline_cmd_file)
	end
	if [ -s $re_search_append_char_file ]
		commandline -i (cat $re_search_append_char_file)
	end
	if [ $res = 0 ]
		commandline -f execute
	end
	rm -f $tmp $re_search_cursor_pos_file $re_search_readline_cmd_file $re_search_append_char_file
end

# vim: tabstop=2 shiftwidth=2 noexpandtab :
