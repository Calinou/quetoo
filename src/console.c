/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "console.h"

#include <signal.h>

console_state_t console_state;

/*
 * @brief Allocates a new `console_string_t`.
 */
static console_string_t *Con_AllocString(int32_t level, const char *string) {

	console_string_t *str = g_new0(console_string_t, 1);
	if (str == NULL) {
		raise(SIGABRT);
	}

	str->level = level;
	str->chars = g_strdup(string ?: "");
	if (str->chars == NULL) {
		raise(SIGABRT);
	}

	str->size = strlen(str->chars);
	str->length = StrColorLen(str->chars);

	str->timestamp = Sys_Milliseconds();

	return str;
}

/*
 * @brief Frees the specified console_str_t.
 */
static void Con_FreeString(console_string_t *str) {

	if (str) {
		g_free(str->chars);
		g_free(str);
	}
}

/*
 * @brief Frees all console_str_t.
 */
static void Con_FreeStrings(void) {

	g_list_free_full(console_state.strings, (GDestroyNotify) Con_FreeString);

	console_state.strings = NULL;
	console_state.size = 0;
}

/*
 * @brief Clears the console buffer.
 */
static void Con_Clear_f(void) {

	Con_FreeStrings();
}

/*
 * @brief Save the console buffer to a file.
 */
static void Con_Dump_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <file_name>\n", Cmd_Argv(0));
		return;
	}

	file_t *file;
	if (!(file = Fs_OpenWrite(Cmd_Argv(1)))) {
		Com_Warn("Couldn't open %s\n", Cmd_Argv(1));
	} else {
		SDL_LockMutex(console_state.lock);

		const GList *list = console_state.strings;
		while (list) {
			const char *c = ((console_string_t *) list->data)->chars;
			while (*c) {
				if (IS_COLOR(c)) {
					c++;
				} else if (!IS_LEGACY_COLOR(c)) {
					if (Fs_Write(file, c, 1, 1) != 1) {
						Com_Warn("Failed to dump console\n");
						break;
					}
				}
				c++;
			}
		}

		SDL_UnlockMutex(console_state.lock);

		Fs_Close(file);
		Com_Print("Dumped console to %s.\n", Cmd_Argv(1));
	}
}

/**
 * @return True if `str` passes the console's filter, false otherwise.
 */
static _Bool Con_Filter(const console_t *console, const console_string_t *str) {

	if (console->level) {
		return console->level & str->level;
	}

	return true;
}

/*
 * @brief Append a message to the console data buffer.
 */
void Con_Append(int32_t level, const char *string) {

	console_string_t *str = Con_AllocString(level, string);

	SDL_LockMutex(console_state.lock);

	console_state.strings = g_list_append(console_state.strings, str);
	console_state.size += str->size;

	while (console_state.size > CON_MAX_SIZE) {
		GList *first = g_list_first(console_state.strings);
		str = first->data;

		console_state.strings = g_list_remove_link(console_state.strings, first);
		console_state.size -= str->size;

		g_list_free_full(first, (GDestroyNotify) Con_FreeString);
	}

	console_state.len = g_list_length(console_state.strings);

	SDL_UnlockMutex(console_state.lock);

	if (console_state.consoles) {

		// iterate the configured consoles and append the new string

		for (GList *list = console_state.consoles; list; list = list->next) {
			const console_t *console = list->data;

			if (console->Append) {
				if (Con_Filter(console, str)) {
					console->Append(str);
				}
			}
		}
	} else {
		char stripped[MAX_PRINT_MSG];
		StripColors(string, stripped);
		puts(stripped);
	}
}

/**
 * @brief Wraps the specified string for the given line width.
 *
 * @param chars The null-terminated C string.
 * @param line_width The desired line width.
 * @param lines The output to store the line offsets.
 * @param max_lines The maximum number of line offsets to store.
 *
 * @return The number of line offsets.
 *
 * @remark If `lines` is `NULL`, this function simply counts the number of
 * wrapped lines in `chars`.
 */
size_t Con_Wrap(const char *chars, size_t line_width, char **lines, size_t max_lines) {

	size_t count = 0;

	const char *line = chars;
	while (*line) {

		size_t width = 0;

		const char *c = line;
		while (*c && width < line_width) {

			if (*c == '\n') {
				break;
			}

			if (IS_COLOR(c)) {
				c += 2;
			} else if (IS_LEGACY_COLOR(c)) {
				c += 1;
			} else {
				c++;
				width++;
			}
		}

		const char *eol = c;

		if (width == line_width) {
			while (!isspace(*eol)) {
				if (eol == line) {
					eol = c;
					break;
				}
				eol--;
			}
		}

		if (lines) {
			if (count < max_lines) {
				lines[count] = g_strndup(line, eol - line);
			} else {
				break;
			}
		}

		line = eol + 1;
		count++;
	}

	return count;
}

/**
 * @brief Tails the console, returning as many as `max_lines` in `lines.
 *
 * @param console The console to tail.
 * @param lines The output to store line offsets.
 * @param max_lines The maximum number of line offsets to store.
 *
 * @return The number of line offsets.
 */
size_t Con_Tail(const console_t *console, char **lines, size_t max_lines) {

	assert(console);

	int32_t back = console->scroll + max_lines;

	GList *start = NULL;
	GList *list = g_list_last(console_state.strings);
	while (list) {
		const console_string_t *str = list->data;

		if (str->timestamp < console->whence)
			break;

		if (Con_Filter(console, str)) {

			back -= Con_Wrap(str->chars, console->width, NULL, 0);

			if (back < 0) {
				if (back < -1)
					list = list->next;
				break;
			}
		}

		start = list;
		list = list->prev;
	}

	size_t count = 0;

	while (start) {
		const console_string_t *str = start->data;

		if (Con_Filter(console, str)) {
			count += Con_Wrap(str->chars, console->width, lines + count, max_lines - count);
		}

		start = start->next;
	}

	return count;
}

/*
 * @brief Navigate history, copying the history line to the input buffer.
 */
void Con_NavigateHistory(console_t *console, console_history_nav_t nav) {

	assert(console);

	console_history_t *hist = &console->history;

	size_t p;
	switch (nav) {
		case CON_HISTORY_PREV:
			p = (hist->pos - 1) % CON_HISTORY_SIZE;
			break;
		case CON_HISTORY_NEXT:
			p = (hist->pos + 1) % CON_HISTORY_SIZE;
			break;
	}

	if (strlen(hist->strings[p])) {
		console_input_t *in = &console->input;

		g_strlcpy(in->buffer, hist->strings[p], sizeof(in->buffer));
		in->pos = strlen(in->buffer);

		hist->pos = p;
	}
}

/**
 * @brief Reads the history log from file into the console's history.
 */
void Con_ReadHistory(console_t *console, file_t *file) {
	char str[MAX_PRINT_MSG];

	assert(console);
	assert(file);

	console_history_t *hist = &console->history;

	while (Fs_ReadLine(file, str, sizeof(str))) {
		g_strlcpy(hist->strings[hist->index++ % CON_HISTORY_SIZE], str, sizeof(str));
	}

	hist->pos = hist->index;
}

/**
 * @brief Writes the history buffer of the first configured console to file.
 */
void Con_WriteHistory(const console_t *console, file_t *file) {

	assert(console);
	assert(file);

	const console_history_t *hist = &console->history;

	for (size_t i = 1; i <= CON_HISTORY_SIZE; i++) {

		const char *str = hist->strings[(hist->index + i) % CON_HISTORY_SIZE];
		if (*str) {
			Fs_Print(file, "%s\n", str);
		} else {
			break;
		}
	}
}

/*
 * @brief Tab completion. Query various subsystems for potential
 * matches, and append an appropriate string to the input buffer. If no
 * matches are found, do nothing. If only one match is found, simply
 * append it. If multiple matches are found, append the longest possible
 * common prefix they all share.
 */
_Bool Con_CompleteInput(console_t *console) {
	const char *pattern, *match;
	GList *matches = NULL;

	char *partial = console->input.buffer;
	if (*partial == '\\' || *partial == '/')
		partial++;

	if (!*partial)
		return false; // lets start with at least something

	// handle special cases for commands which accept filenames
	if (g_str_has_prefix(partial, "demo ")) {
		partial += strlen("demo ");
		pattern = va("demos/%s*.dem", partial);
		Fs_CompleteFile(pattern, &matches);
	} else if (g_str_has_prefix(partial, "exec ")) {
		partial += strlen("exec ");
		pattern = va("%s*.cfg", partial);
		Fs_CompleteFile(pattern, &matches);
	} else if (g_str_has_prefix(partial, "map ")) {
		partial += strlen("map ");
		pattern = va("maps/%s*.bsp", partial);
		Fs_CompleteFile(pattern, &matches);
	} else if (g_str_has_prefix(partial, "set ")) {
		partial += strlen("set ");
		pattern = va("%s*", partial);
		Cvar_CompleteVar(pattern, &matches);
	} else { // handle general case for commands and variables
		pattern = va("%s*", partial);
		Cmd_CompleteCommand(pattern, &matches);
		Cvar_CompleteVar(pattern, &matches);
	}

	if (g_list_length(matches) == 0)
		return false;

	if (g_list_length(matches) == 1)
		match = va("%s ", (char *) g_list_nth_data(matches, 0));
	else
		match = CommonPrefix(matches);

	const size_t len = sizeof(console->input.buffer) - 1;
	g_snprintf(partial, len - (partial - console->input.buffer), "%s", match);
	console->input.pos = strlen(console->input.buffer);

	g_list_free_full(matches, Mem_Free);

	return true;
}

/*
 * @brief Handle buffered input for the specified console, submitting it to the
 * command subsystem, appending it to all configured consoles, and resetting the
 * input state.
 */
void Con_SubmitInput(console_t *console) {

	if (*console->input.buffer) {

		const size_t h = console->history.index++ % CON_HISTORY_SIZE;
		g_strlcpy(console->history.strings[h], console->input.buffer, MAX_PRINT_MSG);

		console->history.pos = console->history.index;

		if (!g_str_has_suffix(console->input.buffer, "\n")) {
			g_strlcat(console->input.buffer, "\n", sizeof(console->input.buffer));
		}

		const char *cmd = console->input.buffer;
		if (*cmd == '\\' || *cmd == '/') {
			cmd++;
		}

		Cbuf_AddText(cmd);
		Con_Append(PRINT_ECHO, cmd);

		memset(&console->input, 0, sizeof(console->input));
	}
}

/*
 * @brief Adds the given console to the configured consoles.
 */
void Con_AddConsole(const console_t *console) {

	SDL_LockMutex(console_state.lock);

	console_state.consoles = g_list_append(console_state.consoles, (gpointer) console);

	SDL_UnlockMutex(console_state.lock);
}

/*
 * @brief Removes the given console from the configured consoles.
 */
void Con_RemoveConsole(const console_t *console) {

	SDL_LockMutex(console_state.lock);

	console_state.consoles = g_list_remove(console_state.consoles, (gpointer) console);

	SDL_UnlockMutex(console_state.lock);
}

/*
 * @brief Initialize the console subsystem. For Windows environments running
 * servers, we explicitly allocate a console and redirect stdio to and from it.
 */
void Con_Init(void) {

	memset(&console_state, 0, sizeof(console_state));

	console_state.lock = SDL_CreateMutex();

	Cmd_Add("clear", Con_Clear_f, 0, NULL);
	Cmd_Add("dump", Con_Dump_f, 0, NULL);
}

/*
 * @brief Shutdown the console subsystem
 */
void Con_Shutdown(void) {

	Cmd_Remove("clear");
	Cmd_Remove("dump");

	Con_FreeStrings();

	g_list_free(console_state.consoles);

	SDL_DestroyMutex(console_state.lock);
}
