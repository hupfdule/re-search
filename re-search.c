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
#define CYAN    "\x1B[36m"
#define BOLD    "\x1B[1m"

#define XSTR(A) STR(A)
#define STR(A) #A

#define MAX_INPUT_LEN 100
#define MAX_LINE_LEN 4096
#define MAX_HISTORY_SIZE (1024 * 256)

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
#define PROMPT(buffer, saved, action, index, result) \
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
		/* print the subsearch */ \
		fprintf(stderr, "%s%s", CYAN, saved); \
		/* print the action  */ \
		fprintf(stderr, "%s<%s> ", action_color, action_str); \
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
			fprintf(stderr, " [%s%s%s%s%s]", NORMAL, BOLD, result, NORMAL, CYAN); \
		} \
		/* restore cursor position */ \
		fprintf(stderr, "\033[u"); \
		/* restore to normal font */ \
		fprintf(stderr, "%s", NORMAL); \
	} while (0)
#endif /* PROMPT */

#ifdef BASH
	#define MIN_CMD_LEN 3
#else
	#define CMD_PREFIX "- cmd: "
	#define CMD_PREFIX_LEN 7
	#define MIN_CMD_LEN 10
#endif /* BASH */

typedef enum {
	SEARCH_BACKWARD, SEARCH_FORWARD, SCROLL, EXECUTE,
} action_t;

/* will be used as exit code to differenciate cases */
typedef enum {
	RESULT_EXECUTE = 0, SEARCH_CANCEL = 1, RESULT_EDIT = 10
} exit_t;

struct termios saved_attributes;
char *history[MAX_HISTORY_SIZE];
char buffer[MAX_INPUT_LEN];
char saved[128];
unsigned long history_size;
int search_result_index;
int substring_index;
FILE *outfile;


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
	char path[1024];
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

#ifdef BASH
/**
 * Parse the bash history file.
 * This is still the legacy code and can probably be accompanied by an
 * additional method that reads from a file that is generated via bashs
 * 'history' command.
 * Every error will return 1, otherwise 0 is returned.
 */
int parse_bash_history() {
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
		if (len < MIN_CMD_LEN)
			continue;

		if (append_to_history(cmdline)) {
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);
	return 0;
}
#endif

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
		if (len < MIN_CMD_LEN)
			continue;

		// skip if pattern not matched
		if (strncmp(CMD_PREFIX, cmdline, CMD_PREFIX_LEN) != 0)
			continue;

		// sanitize
		i = CMD_PREFIX_LEN; j = 0;
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

	// handle sigint for clean exit
	signal(SIGINT, cancel);

	// prepare terminal
	if (set_input_mode())
		exit(EXIT_FAILURE);

	// load history
	int parsing_failed;
#ifdef BASH
	parsing_failed = parse_bash_history();
#else
	parsing_failed = parse_fish_history();
	if (parsing_failed) {
		parsing_failed = parse_fish_history_legacy();
	}
#endif
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
	int i, j;
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
		if (!noop && (buffer_pos > 0 || strlen(saved) > 0)) {
			// FIXME: Where to reset substring_index?
			// search in the history array
			// TODO: factorize?
			if (action == SEARCH_BACKWARD) {
				for (i = search_result_index - 1; i >= 0; i--) {
					char *substring = strstr(history[i], buffer);
					if (substring) {
						search_index++;
						search_result_index = i;
						substring_index = substring - history[i];
						break;
					}
				}
			} else {
				for (i = search_result_index + 1; i < history_size; i++) {
					char *substring = strstr(history[i], buffer);
					if (substring) {
						search_index--;
						search_result_index = i;
						substring_index = substring - history[i];
						break;
					}
				}
			}
		}
		noop = 0;

		// erase line
		fprintf(stderr, "\033[2K\r");

		// print the prompt
		PROMPT(buffer, saved, action,
				search_index,
				search_result_index < history_size ? history[search_result_index] : "");

		fflush(stderr);

		c = getchar();

		switch (c) {
		case 27:
			c = nb_getchar();
			if (c == -1) { // esc
				cancel();
				break;
			} else if (c != 91 && c != 79) {
				ungetc(c, stdin);
				break;
			}

			// multi-characters sequence
			switch (getchar()) {
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
			case 70: // end
			case 67: // right
				accept(RESULT_EDIT);
				break;
			case 72: // home
			case 68: // left
				cancel();
				break;
			}
			break;

		case 4: // C-d
		case 7: // C-g
			cancel();
			break;

		case 5: // C-e
			accept(RESULT_EDIT);
			break;

		case 1: // C-a
			write_readline_function("beginning-of-line");
			accept(RESULT_EDIT);
			break;

		case 12: // C-l
			// erase line
			fprintf(stderr, "\033[2J");
			// jump to upper left corner
			fprintf(stderr, "\033[1;1H");
			break;

		case 10: // enter
		case 13: // new-line
			accept(RESULT_EXECUTE);
			break;

		case 18: //C-r
			action = SEARCH_BACKWARD;
			break;

		case 19: //C-s
			action = SEARCH_FORWARD;
			break;

		case 17: //C-q
			if (strlen(buffer) == 0)
				break;
			j = 0;
			// filter the history array to remove non-matching entries
			for (i = 0; i < history_size; i++) {
				if (strstr(history[i], buffer) && i != j) {
					history[j] = history[i];
					// update the search result index
					if (search_result_index == i)
						search_result_index = j;
					j++;
				} else
					free(history[i]);
			}
			// adjust history size
			history_size = j;
			// add the saved search keyword
			strncat(saved, "[", sizeof(saved) - 1);
			strncat(saved, buffer, sizeof(saved) - 1);
			strncat(saved, "]", sizeof(saved) - 1);
			// reset the buffer
			buffer[0] = '\0';
			buffer_pos = 0;
			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;
			break;

		case 21: // C-u
			buffer[0] = '\0';
			buffer_pos = 0;

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;

			break;

		case 23: // C-w
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

		case 16: // C-p
			if (search_result_index > 0) {
				search_result_index--;
			}

			buffer[0] = '\0';
			buffer_pos = 0;
			action = SCROLL;
			search_index = 0;

			break;

		case 14: // C-n
			if (search_result_index < history_size) {
				search_result_index++;
			}

			buffer[0] = '\0';
			buffer_pos = 0;
			action = SCROLL;
			search_index = 0;

			break;

		case 127: // backspace
		case 8: // backspace
			if (buffer_pos > 0)
				buffer[--buffer_pos] = '\0';

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;

			break;

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

		default:
			// ignore the first 32 non-printing characters
			if (c < 32) {
				noop = 1;
				break;
			}

			// prevent buffer overflow
			if (buffer_pos >= MAX_INPUT_LEN - 1)
				continue;

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
