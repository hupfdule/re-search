set -l current_dir (dirname (status --current-filename))
source $current_dir/../vendor_functions.d/re_search*

bind \cr re_search
bind \cp re_search_scroll
