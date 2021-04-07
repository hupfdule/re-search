function re_search
	set -x fish_history_file (mktemp -t fish.hist.XXXXXX)
  history --null --reverse -n (echo "1024 * 256" | bc) > "$fish_history_file"
	set -l tmp (mktemp -t fish.XXXXXX)
	set -x SEARCH_BUFFER (commandline -b)
	re-search > $tmp
	set -l res $status
	commandline -f repaint
	if [ -s $tmp ]
		commandline -r (cat $tmp)
	end
	if [ $res = 0 ]
		commandline -f execute
	end
	rm -f $tmp $fish_history_file
end
