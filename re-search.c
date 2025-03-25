/*
 * Copyright (C) 2014 Julien Bonjean <julien@bonjean.info>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include "config.h"
#include <sys/wait.h>

#define NORMAL  "\x1B[0m"
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define BLUE    "\x1B[1;34m"
#define CYAN    "\x1B[36m"
#define BOLD    "\x1B[1m"

#define XSTR(A) STR(A)
#define STR(A) #A

#define MAX_INPUT_LEN 100
#define MAX_LINE_LEN 4096
#define MAX_HISTORY_SIZE (1024 * 256)
#define MAX_SUBSEARCHES 8

#ifdef DEBUG
#define debug(fmt, ...) \
	fprintf(stderr, "DEBUG %s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define debug(fmt, ...) \
	do { } while (0)
#endif /* DEBUG */

#define error(fmt, ...) \
	fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#ifndef PROMPT
#define PROMPT(buffer, action, index, result) \
	do { \
		char *action_str; \
		char *action_color; \
		switch (action) { \
			case SEARCH_BACKWARD: \
				action_str = "backward"; \
				action_color = search_index > 0 ? GREEN : RED; \
				break; \
			case SEARCH_FORWARD: \
				action_str = "forward"; \
				action_color = search_index > 0 ? GREEN : RED; \
				break; \
			case SCROLL: \
				action_str = ""; \
				action_color = CYAN; \
				break; \
			case EXECUTE: \
				action_str= "execute"; \
				action_color = CYAN; \
				break; \
			default: \
				action_str = "??"; \
				action_color = RED; \
		} \
		/* print the subsearches */ \
		if (no_of_subsearches > 0) { \
			fprintf(stderr, "%s", CYAN); \
			for (int i= 0; i < no_of_subsearches; i++) { \
				int substr_len = strlen(subsearches[i]); \
				if (subsearches[i][substr_len-1] == '\1'){ \
					char negative_term[substr_len-1]; \
					memcpy(negative_term, subsearches[i], substr_len-1); \
					negative_term[substr_len-1] = '\0'; \
					fprintf(stderr, "[%s!%s%s]", RED, CYAN, negative_term); \
				} else { \
					fprintf(stderr, "[%s]", subsearches[i]); \
				} \
			} \
			fprintf(stderr, "%s", NORMAL); \
		} \
		/* print the action  */ \
		fprintf(stderr, "%s<%s> ", action_color, action_str); \
		/* print the negate marker */ \
		if (negate) { \
			fprintf(stderr, "%s!%s", RED, CYAN); \
		} \
		/* print the search buffer */ \
		fprintf(stderr, "%s%s", CYAN, buffer); \
		/* save cursor position */ \
		fprintf(stderr, "\033[s"); \
		/* if there is a result, append its search index */ \
		if (index > 0) { \
			fprintf(stderr, " (%d)", index); \
		} \
		if (search_result_index < history_size) { \
			/* print the current history entry and the maximum history entry */ \
			fprintf(stderr, " [%lu/%lu]", history_size - search_result_index, history_size); \
		} \
		if (strlen(result) > 0) { \
			/* print the actual result */ \
			/* print opening bracket */ \
			fprintf(stderr, " [%s", NORMAL); \
			/* save cursor position */ \
			fprintf(stderr, "\033[s"); \
			/* print the actual result commandline */ \
			fprintf(stderr, "%s%s%s", BOLD, result, NORMAL); \
			/* print closing bracket */ \
			fprintf(stderr, "%s]", CYAN); \
			/* restore cursor position */ \
			fprintf(stderr, "\033[u"); \
			/* move to found search string */ \
			if (substring_index > -1) { \
				/* Trying to move the cursor 0 chars moved it 1 char instead. */ \
				if (substring_index > 0) { \
					fprintf(stderr, "\033[%iC", substring_index); \
				} \
				/* save cursor position */ \
				fprintf(stderr, "\033[s"); \
				/* overwrite search string in different color */ \
				fprintf(stderr, "%s%s", BLUE, buffer); \
				/* restore cursor position */ \
				fprintf(stderr, "\033[u"); \
			} \
		} else { \
			/* restore cursor position */ \
			fprintf(stderr, "\033[u"); \
		} \
		/* restore to normal font */ \
		fprintf(stderr, "%s", NORMAL); \
	} while (0)
#endif /* PROMPT */

#define BASH_MIN_CMD_LEN     3
#define FISH_CMD_PREFIX      "- cmd: "
#define FISH_CMD_PREFIX_LEN  7
#define FISH_MIN_CMD_LEN     BASH_MIN_CMD_LEN + FISH_CMD_PREFIX_LEN

typedef enum {
	SEARCH_BACKWARD, SEARCH_FORWARD, SCROLL, EXECUTE,
} action_t;

/* will be used as exit code to differenciate cases */
typedef enum {
	RESULT_EXECUTE = 0, SEARCH_CANCEL = 1, RESULT_EDIT = 10
} exit_t;

typedef enum {
	FISH, BASH,
} shell_t;

struct termios saved_attributes;
char *history[MAX_HISTORY_SIZE];
char buffer[MAX_INPUT_LEN];
unsigned long history_size;
int search_result_index;
char subsearches[MAX_SUBSEARCHES][MAX_INPUT_LEN];
int no_of_subsearches;
int substring_index;
int negate;
FILE *outfile;
shell_t shell;


void reset_input_mode() {
	debug("restore terminal settings");

	tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
}

int set_input_mode() {
	debug("setup terminal");

	struct termios tattr;

	// make sure stdin is a terminal
	if (!isatty(STDIN_FILENO)) {
		error("not a terminal");
		return 1;
	}

	// save the terminal attributes so we can restore them later
	tcgetattr(STDIN_FILENO, &saved_attributes);
	atexit(reset_input_mode);

	// set the funny terminal modes
	tcgetattr(STDIN_FILENO, &tattr);
	tattr.c_lflag &= ~(ICANON | ECHO);
	tattr.c_cc[VMIN] = 1;
	tattr.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);

	return 0;
}

FILE *try_open_history(const char *local_path, const char *filename) {
	char path[1152];
	snprintf(path, sizeof(path), "%s/%s/%s", getenv("HOME"), local_path, filename);
	return fopen(path, "r");
}

int append_to_history(const char *cmdline) {
	if (history_size >= MAX_HISTORY_SIZE) {
		debug("maximum history size of %i reached. Dropping oldest history entry: %s", MAX_HISTORY_SIZE, history[0]);
		free(history[0]);
		for (int i = 1; i < history_size; i++) {
			history[i-1] = history[i];
		}
		history[history_size] = NULL; //empty the last element pointer
		history_size--;
	}

	// append to history array
	int len = strlen(cmdline);
	history[history_size] = malloc(len + 1);
	if (!history[history_size]) {
		error("cannot allocate memory");
		return 1;
	}
	strncpy(history[history_size], cmdline, len + 1);
	history_size++;

	return 0;
}


/**
 * Parse the bash history file.
 * This is still the legacy code and can probably be accompanied by an
 * additional method that reads from a file that is generated via bashs
 * 'history' command.
 * Every error will return 1, otherwise 0 is returned.
 */
int parse_bash_history_legacy() {
	char *histpath;
	char  histfile[1024];
	FILE *fp;
	char  cmdline[MAX_LINE_LEN];
	int   len;

	histpath = ".";
	snprintf(histfile, sizeof(histfile), ".bash_history");
	fp = try_open_history(histpath, histfile);

	if (!fp) {
		error("cannot open history file %s/%s/%s: %s", getenv("HOME"), histpath, histfile, strerror(errno));
		return 1;
	}

	// parse the history file
	while (fgets(cmdline, sizeof(cmdline), fp) != NULL) {

		// skip if truncated
		len = strlen(cmdline);
		if (len == 0 || cmdline[len - 1] != '\n')
			continue;
		cmdline[--len] = 0; // remove \n

		// skip if too short
		if (len < BASH_MIN_CMD_LEN)
			continue;

		if (append_to_history(cmdline)) {
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);
	return 0;
}

/**
 * Parse the bash history file specified via $bash_history_file.
 * The files content _must_ be one command per line. It is _not_
 * the direct output of `history` as that command prefixes each
 * command with an id. This id (and the surrounding whitespace)
 * must be stripped first.
 * And it should already be limited to a length of MAX_HISTORY_SIZE.
 * If the environment variable $bash_history_file does not exist,
 * this method returns 1. If reading the file failed, it returns 2.
 * If appending all the entries to the history fails, it returns 3.
 * Otherwise it returns 0.
 */
int parse_bash_history() {
	char *bash_history_file = getenv("bash_history_file");
	if (bash_history_file == NULL) {
		debug("$bash_history_file is not set");
		return 1;
	}

	FILE *history_file= fopen(bash_history_file, "r");
	if (history_file == NULL) {
		error("error reading bash_history_file %s: %s", bash_history_file, strerror(errno));
		return 2;
	}

	char  cmdline[MAX_LINE_LEN];
	int   len= 0;

	while (fgets(cmdline, sizeof(cmdline), history_file) != NULL) {
		// skip if truncated
		len = strlen(cmdline);
		if (len == 0 || cmdline[len - 1] != '\n')
			continue;
		cmdline[--len] = 0; // remove \n

		// skip if too short
		if (len < BASH_MIN_CMD_LEN)
			continue;

		if (append_to_history(cmdline)) {
			fclose(history_file);
			return 1;
		}
	}

	fclose(history_file);
	return 0;
}


/**
 * Parse the fish history file via legacy code by reading from the file
 * fish writes its history to. This has the drawback that it contains
 * entries from other fish instances and there does not fully resemble the
 * currents shell history. For that reason 'parse_fish_history()' is
 * preferred.
 * However, if 'parse_fish_history' fails or cannot be used, because the
 * $fish_history_file environment variable is not set, this method can
 * be used as fallback.
 * Every error will return 1, otherwise 0 is returned.
 */
int parse_fish_history_legacy() {
	char *histpath;
	char  histfile[1024];
	FILE *fp;
	char  cmdline[MAX_LINE_LEN];
	int   i,j,len;

	char *fish_session = getenv("fish_history");
	if (!fish_session) {
		fish_session = "fish";
	}
	snprintf(histfile, sizeof(histfile), "%s_history", fish_session);

	histpath = ".local/share/fish";
	fp = try_open_history(histpath, histfile);
	if (!fp && errno == ENOENT) {
		debug("cannot open history file %s/%s/%s: %s", getenv("HOME"), histpath, histfile, strerror(errno));
		histpath = ".config/fish";
		fp = try_open_history(histpath, histfile);
	}

	if (!fp) {
		error("cannot open history file %s/%s/%s: %s", getenv("HOME"), histpath, histfile, strerror(errno));
		return 1;
	}

	// parse the history file
	while (fgets(cmdline, sizeof(cmdline), fp) != NULL) {

		// skip if truncated
		len = strlen(cmdline);
		if (len == 0 || cmdline[len - 1] != '\n')
			continue;
		cmdline[--len] = 0; // remove \n

		// skip if too short
		if (len < FISH_MIN_CMD_LEN)
			continue;

		// skip if pattern not matched
		if (strncmp(FISH_CMD_PREFIX, cmdline, FISH_CMD_PREFIX_LEN) != 0)
			continue;

		// sanitize
		i = FISH_CMD_PREFIX_LEN; j = 0;
		while (i < len) {
			if (j > 0 && cmdline[i] == '\\' && cmdline[j-1] == '\\') {
				j--;
			} else if (i < (len -1) && cmdline[i] == '\\' && cmdline[i + 1] == 'n') {
				cmdline[j] = '\n';
				i++;
			} else {
				cmdline[j] = cmdline[i];
			}

			i++; j++;
		}
		cmdline[j] = '\0';
		len = j;

		if (append_to_history(cmdline)) {
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);
	return 0;
}

/**
 * Parse the fish history file specified via $fish_history_file.
 * The files content _must_ be the output of 'history --null --reverse'.
 * And it should already be limited via '-n' to MAX_HISTORY_SIZE.
 * If the environment variable $fish_history_file does not exist,
 * this method returns 1. If reading the file failed, it returns 2.
 * If appending all the entries to the history fails, it returns 3.
 * Otherwise it returns 0.
 */
int parse_fish_history() {
	char *fish_history_file = getenv("fish_history_file");
	if (fish_history_file == NULL) {
		debug("$fish_history_file is not set");
		return 1;
	}

	FILE *history_file= fopen(fish_history_file, "r");
	if (history_file == NULL) {
		error("error reading fish_history_file %s: %s", fish_history_file, strerror(errno));
		return 2;
	}

	char cmdline[MAX_LINE_LEN];
	int ch;
	int len= 0;
	int too_long= 0; // set to 1 if a commandline from the history is longer than MAX_LINE_LEN

	while((ch=fgetc(history_file)) != EOF) {
		if (len >= MAX_LINE_LEN - 1 && !too_long) {
			too_long= 1;
			len++;
			continue;
		}

		if (ch == '\0') {
			if (too_long) {
				too_long = 0;
				cmdline[len]='\0';
				debug("commandline is too long (more than %i chars): %s", strlen(cmdline), cmdline);
			} else {
				cmdline[len]= '\0';
				if (append_to_history(cmdline)) {
					fclose(history_file);
					return 3;
				}
			}
			len= 0;
		} else if (!too_long) {
			cmdline[len++] = ch;
		}
	}

	fclose(history_file);
	return 0;
}

#ifdef CHECK_DUPLICATES
/**
 * Remove duplicate entries from the already parsed history.
 */
void remove_duplicates() {
	int i,j,k,dup;

	// check for duplicates, it is easier to do it afterward to preserve
	// history order
	k = 0;
	for (i = 0 ; i < history_size ; i++) {
		dup = 0;
		for (j = i + 1 ; j < history_size ; j++) {
			if (!strcmp(history[i], history[j])) {
				dup = 1;
				break;
			}
		}
		if (dup) {
			free(history[i]);
		} else {
			if (i != k)
				history[k] = history[i];
			k++;
		}
	}
	// adjust history size
	history_size = k;
}
#endif

void execute(char *cmdline) {
	// Execute commandline in a separate child process
	int cpid = fork();
	if (cpid == 0) {
		reset_input_mode();
		// print the line to execute
		fprintf(stdout, "%s$ %s%s\n", CYAN, cmdline, NORMAL);
		// then execute it
		execl(getenv("SHELL"), getenv("SHELL"), "-c", cmdline, NULL);
	}
	wait(&cpid);
	set_input_mode();
}

void restore_terminal() {
	// reset color
	fprintf(stderr, "%s", NORMAL);

	// clear last printed results
	fprintf(stderr, "\33[2K\r");

	// restore line wrapping
	fprintf(stderr, "\033[?7;h");

	fflush(stderr);
}

int nb_getchar() {
	int c;

	// mark stdin nonblocking
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	c = getchar();

	// restore stdin
	fcntl(STDIN_FILENO, F_SETFL, flags);

	return c;
}

void free_history() {
	int i = 0;
	for (i = 0; i < history_size; i++)
		free(history[i]);

	// just to be ensure we won't free again
	history_size = 0;
}

void cleanup() {
	debug("cleanup resources");

	restore_terminal();
	free_history();
}

void accept(int status) {
	// print result/buffer to stdout
	fprintf(outfile, "%s",
			search_result_index < history_size ?
					history[search_result_index] : buffer);
	cleanup();
	exit(status);
}

void cancel() {
	cleanup();
	exit(SEARCH_CANCEL);
}

int matches_all_searches(char *history_entry) {
	for (int i=0; i < no_of_subsearches; i++) {
		int substr_len = strlen(subsearches[i]);
		int negative = 0;
		char negatives[substr_len-1];
		if (subsearches[i][substr_len-1] == '\1') {
			// negative substring
			memcpy(negatives, subsearches[i], substr_len-1);
			negatives[substr_len-1] = '\0';
			negative = 1;
		}

		if (!negative && !strstr(history_entry, subsearches[i])) {
			return 0;
		} else if (negative && strstr(history_entry, negatives)) {
			return 0;
		}
	}

	if (!negate && strstr(history_entry, buffer)) {
		return 1;
	} else if (negate && !strstr(history_entry, buffer)) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * Writes the specified in to the file denoted by
 * $fish_cursor_pos_file. But only if it is >= 0.
 *
 * If the environment variable $fish_cursor_pos_file is not set, it does
 * nothing.
 */
void write_readline_position(const int readline_position) {
	if (readline_position < 0) {
		return;
	}

	char *readline_pos_file = getenv("fish_cursor_pos_file");
	if (readline_pos_file != NULL) {
		FILE *fp= fopen(readline_pos_file, "w");
		if (fp != NULL) {
			fprintf(fp, "%d", readline_position);
			fclose(fp);
		}
	}
}

/**
 * Writes the specified string to the file denoted by
 * $fish_readline_cmd_file. The string should be a valid readline movement
 * function name that can be correctly interpreted by fish.
 *
 * If the environment variable $fish_readline_cmd_file is not set, it does
 * nothing.
 */
void write_readline_function(const char *readline_function) {
	char *readline_function_file = getenv("fish_readline_cmd_file");
	if (readline_function_file != NULL) {
		FILE *fp= fopen(readline_function_file, "w");
		if (fp != NULL) {
			fputs(readline_function, fp);
			fclose(fp);
		}
	}
}


/**
 * Writes the specified char to the file denoted by
 * $fish_append_char_file.
 *
 * If the environment variable $fish_append_char is not set, it does
 * nothing.
 */
void write_append_char(const int c) {
	char *append_char_file = getenv("fish_append_char_file");
	if (append_char_file != NULL) {
		FILE *fp= fopen(append_char_file, "w");
		if (fp != NULL) {
			fprintf(fp, "%c", c);
			fclose(fp);
		}
	}
}


void check_shell() {
	char* re_search_shell = getenv("RE_SEARCH_SHELL");
	if (re_search_shell != NULL && strcmp(re_search_shell , "BASH") == 0) {
		debug("Shell is BASH");
		shell = BASH;
	} else {
		debug("Shell is FISH");
		shell = FISH;
	}
}


int main(int argc, char **argv) {
	// write either to stdout or the given file
	if (argc == 1) {
		outfile = stdout;
	} else if (argc == 2) {
		outfile = fopen(argv[1], "w");
	} else {
		error("The only (optional) valid parameter is the path to the file to write the result to.");
		return 1;
	}

	// ensure everything is initialized
	buffer[0] = '\0';
	history_size = 0;
	no_of_subsearches = 0;
	negate = 0;

	// handle sigint for clean exit
	signal(SIGINT, cancel);

	// prepare terminal
	if (set_input_mode())
		exit(EXIT_FAILURE);

	// check which shells history to use
	check_shell();

	// load history
	int parsing_failed;
	if (shell == BASH) {
		parsing_failed = parse_bash_history();
		if (parsing_failed) {
			parsing_failed = parse_bash_history_legacy();
		}
	} else {
		parsing_failed = parse_fish_history();
		if (parsing_failed) {
			parsing_failed = parse_fish_history_legacy();
		}
	}

	if (parsing_failed) {
		free_history();
		exit(EXIT_FAILURE);
	}

#ifdef CHECK_DUPLICATES
	remove_duplicates();
#endif

	debug("%d entries loaded", history_size);

	char c;

	int search_index = 0;
	int buffer_pos = 0;
	int i;
	action_t action = SEARCH_BACKWARD;
	search_result_index = history_size;

	// if the buffer environment variable is set, populate the input buffer
	char* env_buffer = getenv("SEARCH_BUFFER");
	if (env_buffer && strlen(env_buffer) > 0) {
		strncpy(buffer, env_buffer, sizeof(buffer) - 1);
		buffer[sizeof(buffer) - 1] = '\0';
		buffer_pos = strlen(buffer);

		// remove trailing '\n'
		if (buffer[buffer_pos - 1] == '\n')
			buffer[--buffer_pos] = '\0';
	}

	// if the start index environment variable is set, jump to the
	// corresponding history entry
	char* start_index = getenv("START_INDEX");
	if (start_index && strlen(start_index) > 0) {
		int idx= strtol(start_index, NULL, 10);
		search_result_index = history_size - idx;
		action = SCROLL;

		// ignore $SEARCH_BUFFER
		buffer[0] = '\0';
		buffer_pos = 0;
	}

	// disable line wrapping
	fprintf(stderr, "\033[?7l");

	int noop = 0;
	while (1) {
		// place cursor at end of current history item (if there is one)
		// in case a search string is entered it will be repositioned later
		if (search_result_index < history_size) {
			substring_index = strlen(history[search_result_index]);
		} else {
			substring_index = 0;
		}

		if (!noop && (buffer_pos > 0 || no_of_subsearches > 0)) {
			// FIXME: Where to reset substring_index?
			// search in the history array
			// TODO: factorize?
			if (action == SEARCH_BACKWARD) {
				for (i = search_result_index - 1; i >= 0; i--) {
					if (matches_all_searches(history[i])) {
						search_index++;
						search_result_index = i;
						if (negate) {
							substring_index = -1;
						} else {
							char *substring = strstr(history[i], buffer);
							if (substring) {
								substring_index = substring - history[i];
							}
						}
						break;
					}
				}
			} else {
				for (i = search_result_index + 1; i < history_size; i++) {
					if (matches_all_searches(history[i])) {
						search_index--;
						search_result_index = i;
						char *substring = strstr(history[i], buffer);
						if (substring) {
							substring_index = substring - history[i];
						}
						break;
					}
				}
			}
		}
		noop = 0;

		// erase line
		fprintf(stderr, "\033[2K\r");

		// print the prompt
		PROMPT(buffer, action,
				search_index,
				search_result_index < history_size ? history[search_result_index] : "");

		fflush(stderr);

		c = getchar();
		debug("\n1st received key: %i", c);

		switch (c) {
		case 27:
			// Alt- key combinations
			c = nb_getchar();
			debug("\n2nd received key: %i", c);
			switch (c) {
				case -1: // esc
					cancel();
					break;
				case 98: // Alt-b
					write_readline_position(substring_index);
					write_readline_function("backward-word");
					accept(RESULT_EDIT);
					break;
				case 102: // Alt-f
					write_readline_position(substring_index);
					write_readline_function("forward-word");
					accept(RESULT_EDIT);
					break;
				case 100: // Alt-d
					write_readline_position(substring_index);
					write_readline_function("kill-word");
					accept(RESULT_EDIT);
					break;
				case 127: // Alt-Backspace (Alt-DEL)
					write_readline_position(substring_index);
					write_readline_function("backward-kill-word");
					accept(RESULT_EDIT);
					break;
				case 116: // Alt-t
					write_readline_position(substring_index);
					write_readline_function("transpose-words");
					accept(RESULT_EDIT);
					break;
				case 117: // Alt-u
					write_readline_position(substring_index);
					write_readline_function("upcase-word");
					accept(RESULT_EDIT);
					break;
				case 108: // Alt-l
					write_readline_position(substring_index);
					write_readline_function("downcase-word");
					accept(RESULT_EDIT);
					break;
				case 99: // Alt-c
					write_readline_position(substring_index);
					write_readline_function("capitalize-word");
					accept(RESULT_EDIT);
					break;
			}

			// multi-characters sequence
			c = getchar();
			debug("\n3rd received key: %i", c);
			switch (c) {
			case 53: // pg-up
				getchar();
				/* no break */
			case 65: // up
				action = SEARCH_BACKWARD;
				break;
			case 54: // pg-down
				getchar();
				/* no break */
			case 66: // down
				action = SEARCH_FORWARD;
				break;
			case 52: // end
				accept(RESULT_EDIT);
				break;
			case 67: // right
				write_readline_position(substring_index);
				write_readline_function("forward-char");
				accept(RESULT_EDIT);
				break;
			case 49: // home
				write_readline_function("beginning-of-line");
				accept(RESULT_EDIT);
				break;
			case 68: // left
				write_readline_position(substring_index);
				write_readline_function("backward-char");
				accept(RESULT_EDIT);
				break;
			}
			break;

		// Abort the search
		case 4: // C-d
		case 7: // C-g
			cancel();
			break;

		// Accept current history entry & place cursor at the end
		case 5: // C-e
			accept(RESULT_EDIT);
			break;

		// Accept current history entry & move cursor 1 char to the left
		case 2: // C-b
			write_readline_position(substring_index);
			write_readline_function("backward-char");
			accept(RESULT_EDIT);
			break;

		// Accept current history entry & move cursor 1 char to the right
		case 6: // C-f
			write_readline_position(substring_index);
			write_readline_function("forward-char");
			accept(RESULT_EDIT);
			break;

		// Accept current history entry & kill to the end of the line
		case 11: // C-k
			write_readline_position(substring_index);
			write_readline_function("kill-line");
			accept(RESULT_EDIT);
			break;

		// Accept current history entry & place cursor at the beginning
		case 1: // C-a
			write_readline_function("beginning-of-line");
			accept(RESULT_EDIT);
			break;

		// Clear the screen
		case 12: // C-l
			// erase line
			fprintf(stderr, "\033[2J");
			// jump to upper left corner
			fprintf(stderr, "\033[1;1H");
			break;

		// Execute the current history entry
		case 10: // enter
		case 13: // new-line
			accept(RESULT_EXECUTE);
			break;

		// Search backward
		case 18: //C-r
			action = SEARCH_BACKWARD;
			break;

		// Search forward
		case 19: //C-s
			action = SEARCH_FORWARD;
			break;

		// Add the current search term to the subsearches
		case 17: //C-q
			if (strlen(buffer) == 0)
				break;

			if (negate) {
				buffer[buffer_pos] = '\1';
				buffer[++buffer_pos] = '\0';
				negate = 0;
			}

			strcpy(subsearches[no_of_subsearches], buffer);
			no_of_subsearches++;

			// reset the buffer
			buffer[0] = '\0';
			buffer_pos = 0;
			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;
			break;

		// Negate the search term
		case 46: //C-.
			negate = !negate;
			debug("negate search: %i", negate);

			break;

		// Delete the last subsearch term
		case 24: //C-x
			if (no_of_subsearches == 0)
				break;

			no_of_subsearches--;

			break;

		// Delete from cursor to beginning of line
		case 21: // C-u
			// when scrolling or executing from history end re-search and execute Ctrl-u
			if (action == SCROLL || action == EXECUTE) {
				write_readline_function("backward-kill-line");
				accept(RESULT_EDIT);
				break;
			}

			// otherwise delete the whole search string
			buffer[0] = '\0';
			buffer_pos = 0;

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;

			break;

		// Delete from cursor to beginning of word
		case 23: // C-w
			// when scrolling or executing from history end re-search and execute Ctrl-w
			if (action == SCROLL || action == EXECUTE) {
				write_readline_function("backward-kill-word");
				accept(RESULT_EDIT);
				break;
			}

			// otherwise delete last word of search
			i = 0; // i == 1 means we have reached a non-space character
			while (buffer_pos > 0) {
				if (buffer[--buffer_pos] != ' ') {
					i= 1;
				} else if (i == 1) {
					// stop at next space
					buffer_pos++;
					break;
				}
			}
			buffer[buffer_pos] = '\0';

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;

			break;

		// Scroll to previous history entry
		case 16: // C-p
			if (search_result_index > 0) {
				search_result_index--;
			}

			buffer[0] = '\0';
			buffer_pos = 0;
			action = SCROLL;
			search_index = 0;

			break;

		// Scroll to next history entry
		case 14: // C-n
			if (search_result_index < history_size) {
				search_result_index++;
			}

			buffer[0] = '\0';
			buffer_pos = 0;
			action = SCROLL;
			search_index = 0;

			break;

		// Delete previous character
		case 127: // backspace
		case 8: // backspace
			// when scrolling or executing from history end re-search and execute Ctrl-h
			if (action == SCROLL || action == EXECUTE) {
				write_readline_function("backward-delete-char");
				accept(RESULT_EDIT);
				break;
			}

			// otherwise delete last word of search
			if (buffer_pos > 0)
				buffer[--buffer_pos] = '\0';

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;

			break;

		// Execute the search term and jump to the next one
		case 15: // ctrl-o
			restore_terminal();
			char *cmdline = search_result_index < history_size ? history[search_result_index] : buffer;

			execute(cmdline);
			append_to_history(cmdline);

			// we don't need the buffer contents in execute mode
			buffer[0] = '\0';
			buffer_pos = 0;

			// jump to next history entry
			action = EXECUTE;
			search_result_index++;
			search_index = 0;

			break;

		// Append a character to the search term
		default:
			// ignore the first 32 non-printing characters
			if (c < 32) {
				noop = 1;
				break;
			}

			// prevent buffer overflow
			if (buffer_pos >= MAX_INPUT_LEN - 1)
				continue;

			// entering a printable character while scrolling ends the scrolling
			// and appends the new character
			if (action == SCROLL) {
				write_append_char(c);
				accept(RESULT_EDIT);
				break;
			}

			buffer[buffer_pos] = c;
			buffer[++buffer_pos] = '\0';

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;
		}
	}

	error("should not happen");
	cancel();
}

// vim: tabstop=2 shiftwidth=2 noexpandtab :
