#include "file.h"
#include "common.h"
#include "mpq.h"
#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>

#define STORM_FILE_METATABLE "Storm File"

extern struct Storm_File
*storm_file_initialize (lua_State *L)
{
	struct Storm_File *file = lua_newuserdata (L, sizeof (*file));

	file->handle = NULL;
	file->is_writable = 0;
	file->write_position = 0;

	luaL_setmetatable (L, STORM_FILE_METATABLE);

	return file;
}

extern struct Storm_File
*storm_file_access (lua_State *L, int index)
{
	return luaL_checkudata (L, index, STORM_FILE_METATABLE);
}

/**
 * `file:size ()`
 *
 * Returns the size (a `number`) of the open file.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
file_size (lua_State *L)
{
	struct Storm_File *file = storm_file_access (L, 1);
	DWORD size = SFileGetFileSize (file->handle, NULL);

	if (size == SFILE_INVALID_SIZE)
	{
		return storm_result (L, 0);
	}

	lua_pushinteger (L, (lua_Integer) size);

	return 1;
}

/**
 * `file:seek ([whence [, offset]])`
 *
 * Sets and gets the file position, measured from the beginning of the file,
 * to the position given by `offset (number)` plus a base specified by
 * `whence (string)`, as follows:
 *
 * - `"set"`: Base is position `0` (i.e. the beginning of the file).
 * - `"cur"`: Base is the current position.
 * - `"end"`: Base is the end of the file.  For writable files, this is the
 *   last written position (not the absolute file size).
 *
 * In case of success, this function returns the final file position,
 * measured in bytes from the beginning of the file.  Otherwise, it returns
 * `nil`, a `string` describing the error, and a `number` indicating the
 * error code.
 *
 * * The default value for `whence` is `"cur"`, and for offset is `0`.
 * Therefore, the call `file:seek ()` returns the current file position,
 * without changing it; the call `file:seek ('set')` sets the position to
 * the beginning of the file (and returns `0`); and the call `file:seek
 * ('end')` sets the position to the end of the file, and returns its size.
 */
static int
file_seek (lua_State *L)
{
	static const int
	modes [] = {
		FILE_BEGIN,
		FILE_CURRENT,
		FILE_END
	};

	static const char *const
	mode_options [] = {
		"set",
		"cur",
		"end",
		NULL
	};

	const struct Storm_File *file = storm_file_access (L, 1);
	int option = luaL_checkoption (L, 2, "cur", mode_options);
	LONG offset = (LONG) luaL_optinteger (L, 3, 0);

	int mode = modes [option];
	DWORD position;

	/*
	 * For write enabled files, StormLib will return the the file size when
	 * `FILE_END` is used.  However, we want to consider the last position
	 * written to be the end of the file.
	 */
	if (file->is_writable && mode == FILE_END)
	{
		position = SFileSetFilePointer
			(file->handle, file->write_position, NULL, FILE_BEGIN);

		if (position == SFILE_INVALID_POS)
		{
			goto error;
		}

		/* A positive offset takes up beyond the last position written. */
		if (offset > 0)
		{
			offset = 0;
		}

		/* We are at the 'end' of the file now. */
		mode = FILE_CURRENT;
	}

	position = SFileSetFilePointer (file->handle, offset, NULL, mode);

	if (position == SFILE_INVALID_POS)
	{
		goto error;
	}

	/* Never go beyond the last position written. */
	if (file->is_writable && position > file->write_position)
	{
		position = SFileSetFilePointer (
			file->handle, file->write_position, NULL, FILE_BEGIN);

		if (position == SFILE_INVALID_POS)
		{
			goto error;
		}
	}

	lua_pushinteger (L, (lua_Integer) position);

	return 1;

error:
	return storm_result (L, 0);
}

static int
file_read_line (lua_State *L, const struct Storm_File *file, int chop)
{
	luaL_Buffer line;

	char character = '\0';
	int status = 0;

	luaL_buffinit (L, &line);

	do
	{
		/* NOLINTNEXTLINE(misc-sizeof-expression) */
		char *buffer = luaL_prepbuffer (&line);
		int index = 0;

		do
		{
			status = SFileReadFile (file->handle, buffer, 1, NULL, NULL);

			if (!status)
			{
				break;
			}

			character = *buffer++;
		}
		/* NOLINTNEXTLINE(misc-sizeof-expression) */
		while (character != '\n' && ++index < LUAL_BUFFERSIZE);

		luaL_addsize (&line, index);
	}
	while (status && character != '\n');

	if (!chop && character == '\n')
	{
		luaL_addchar (&line, character);
	}

	luaL_pushresult (&line);

	return status || lua_rawlen (L, -1) > 0;
}

static int
file_read_characters (lua_State *L,
	const struct Storm_File *file, size_t count)
{
	luaL_Buffer characters;
	DWORD bytes_to_read;
	DWORD bytes_read;

	int status = 0;

	luaL_buffinit (L, &characters);

	do
	{
		char *buffer;

		/* NOLINTNEXTLINE(misc-sizeof-expression) */
		bytes_to_read = count > LUAL_BUFFERSIZE ? LUAL_BUFFERSIZE : count;
		count -= bytes_to_read;

		buffer = luaL_prepbuffsize (&characters, bytes_to_read);

		status = SFileReadFile (file->handle, buffer,
			bytes_to_read, &bytes_read, NULL);

		luaL_addsize (&characters, bytes_read);
	}
	while (count > 0 && status);

	luaL_pushresult (&characters);

	return status || bytes_read > 0;
}

/**
 * `file:read (...)`
 *
 * Reads the file, according to the given formats, which specify what to
 * read.  For each format, the function returns a `string` with the
 * characters read, or `nil` if it cannot read data.  In this latter case,
 * the function does not return subsequent formats.  When called without
 * formats, it uses a default format that reads the next line.
 *
 * The available formats are either a `string` or `number`:
 *
 * - `"a"`: Reads the whole file, starting at the current position.  On end
 *   of file, it returns the empty string.
 * - `"l"`: Reads the next line, skipping the end of line, returning `nil`
 *   on end of file.  This is the default format.
 * - `"L"`: Reads the next line, keeping the end of line character (if
 *   present), returning `nil` on end of file.
 * - `number`: Reads a string with up to this many bytes, returning `nil`
 *   on end of file.  If `number` is zero, it reads nothing and returns an
 *   empty string, or `nil` on end of file.
 *
 * The formats `"l"` and `"L"` should only be used for text files.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
file_read (lua_State *L)
{
	const struct Storm_File *file = storm_file_access (L, 1);

	DWORD size;
	int index;
	int arguments;

	int status = 1;

	if (file->is_writable)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		status = 0;
		goto error;
	}

	size = SFileGetFileSize (file->handle, NULL);

	if (size == SFILE_INVALID_SIZE)
	{
		status = 0;
		goto error;
	}

	index = 1;
	arguments = lua_gettop (L) - index++;

	/* By default, read a line (with no line break) */
	if (arguments == 0)
	{
		arguments++;
		lua_pushliteral (L, "l");
	}

	/* Ensure stack space for all results and the buffer. */
	luaL_checkstack (L, arguments + LUA_MINSTACK, "too many arguments");

	for (; arguments-- && status; index++)
	{
		const char *format;

		if (lua_type (L, index) == LUA_TNUMBER)
		{
			DWORD position = SFileSetFilePointer (
				file->handle, 0, NULL, FILE_CURRENT);

			if (position == SFILE_INVALID_POS)
			{
				status = 0;
				goto error;
			}

			if (position >= size)
			{
				lua_pushnil (L);
				status = 0;
			}
			else
			{
				size_t count = (size_t) luaL_checkinteger (L, index);
				status = file_read_characters (L, file, count);
			}

			continue;
		}

		format = luaL_checkstring (L, index);

		if (*format == '*')
		{
			format++;
		}

		switch (*format)
		{
			case 'l':
			{
				status = file_read_line (L, file, 1);
				break;
			}

			case 'L':
			{
				status = file_read_line (L, file, 0);
				break;
			}

			case 'a':
			{
				file_read_characters (L, file, (size_t) size);
				status = 1;
				break;
			}

			default:
			{
				return luaL_argerror (L, index, "invalid format");
			}

		}
	}

	if (!status)
	{
		lua_pop (L, 1);
		lua_pushnil (L);
	}

	return index - 2;

error:
	return storm_result (L, 0);
}

static int
lines_iterator (lua_State *L)
{
	const struct Storm_File *file =
		storm_file_access (L, lua_upvalueindex (1));

	DWORD size;
	DWORD position;

	int index;
	int arguments;
	int results;

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);

		results = storm_result (L, 0);
		goto error;
	}

	size = SFileGetFileSize (file->handle, NULL);

	if (size == SFILE_INVALID_SIZE)
	{
		results = storm_result (L, 0);
		goto error;
	}

	position = SFileSetFilePointer (file->handle, 0, NULL, FILE_CURRENT);

	if (position == SFILE_INVALID_POS)
	{
		results = storm_result (L, 0);
		goto error;
	}

	if (position >= size)
	{
		results = 0;
		goto error;
	}

	lua_settop (L, 0);
	lua_pushvalue (L, lua_upvalueindex (1));

	arguments = (int) lua_tointeger (L, lua_upvalueindex (2));
	luaL_checkstack (L, arguments, "too many arguments");

	for (index = 1; index <= arguments; index++)
	{
		lua_pushvalue (L, lua_upvalueindex (2 + index));
	}

	results = file_read (L);
	lua_assert (results > 0);

	/* If the first result is not `nil`, return all results. */
	if (lua_toboolean (L, -results))
	{
		return results;
	}

error:

	/* Is there error information? */
	if (results > 1)
	{
		return luaL_error (L, "%s", lua_tostring (L, -results + 1));
	}

	/* Otherwise, this should only mean EOF. */
	return 0;
}

#define LINES_MAXIMUM_ARGUMENTS 250

/**
 * `file:lines (...)`
 *
 * Returns an interator `function` that, each time it is called, reads the
 * file according to the given formats.  When no format is given, uses `"l"`
 * as a default.  For details on the available formats, see `file:read ()`.
 *
 * In case of errors this function raises the error, instead of returning an
 * error code.
 */
static int
file_lines (lua_State *L)
{
	const struct Storm_File *file = storm_file_access (L, 1);

	int arguments;

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		return storm_result (L, 0);
	}

	arguments = lua_gettop (L) - 1;

	luaL_argcheck (L, arguments <= LINES_MAXIMUM_ARGUMENTS,
		LINES_MAXIMUM_ARGUMENTS + 2, "too many arguments");

	lua_pushinteger (L, arguments);
	lua_insert (L, 2);

	lua_pushcclosure (L, lines_iterator, 2 + arguments);

	return 1;
}

/**
 * `file:write (...)`
 *
 * Writes the value of each of its arguments to the end of `file`.  The
 * arguments must be `string` or `number`.  Note that written data is always
 * appened to the file.
 *
 * An error will be returned if the amount of data written to the file
 * exceeds the size specified upon creation.
 *
 * In the case of success, this function returns `file`.  Otherwise, it
 * returns `nil`, a `string` describing the error, and a `number` indicating
 * the error code.
 */
static int
file_write (lua_State *L)
{
	struct Storm_File *file = storm_file_access (L, 1);

	int index = 1;
	int arguments = lua_gettop (L) - index++;
	int status = 0;

	if (!file->is_writable)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	if (SFileSetFilePointer (file->handle,
		file->write_position, NULL, FILE_BEGIN) == SFILE_INVALID_POS)
	{
		goto error;
	}

	for (; arguments--; index++)
	{
		size_t size;
		const char *text = luaL_checklstring (L, index, &size);

		status = SFileWriteFile (file->handle,
			text, (DWORD) size, MPQ_COMPRESSION_ZLIB);

		if (!status)
		{
			goto error;
		}

	}

	file->write_position = SFileSetFilePointer (
		file->handle, 0, NULL, FILE_CURRENT);

	if (file->write_position == SFILE_INVALID_POS)
	{
		goto error;
	}

	lua_settop (L, 1);
	return 1;

error:
	return storm_result (L, status);
}

/**
 * `file:close ()`
 *
 * Returns a `boolean` indicating that the file was successfully closed.
 *
 * For files opened with write mode this function flushes any data that
 * remains after previous calls of `file:write ()`.  If the amount of data
 * does not match the size specified upon creation, an error will be
 * returned.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
file_close (lua_State *L)
{
	struct Storm_File *file = storm_file_access (L, 1);
	int status = 0;

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
	}
	else if (file->is_writable)
	{
		status = SFileFinishFile (file->handle);
	}
	else
	{
		status = SFileCloseFile (file->handle);
	}

	file->handle = NULL;

	return storm_result (L, status);
}

/**
 * `file:__tostring ()`
 *
 * Returns a `string` representation of the `Storm File` object, indicating
 * whether it is closed, open for writing, or open for reading.
 */
static int
file_to_string (lua_State *L)
{
	const struct Storm_File *file = storm_file_access (L, 1);
	const char *text;

	if (!file->handle)
	{
		text = "%s (%p) (Closed)";
	}
	else
	{
		text = "%s (%p)";
	}

	lua_pushfstring (L, text, STORM_FILE_METATABLE, file);

	return 1;
}

static const luaL_Reg
file_methods [] =
{
	{ "size", file_size },
	{ "seek", file_seek },
	{ "read", file_read },
	{ "lines", file_lines },
	{ "write", file_write },
	{ "close", file_close },
	{ "__tostring", file_to_string },
	{ "__gc", file_close },
	{ NULL, NULL }
};

extern void
storm_file_metatable (lua_State *L)
{
	luaL_newmetatable (L, STORM_FILE_METATABLE);
	luaL_setfuncs (L, file_methods, 0);

	lua_pushvalue (L, -1);
	lua_setfield (L, -2, "__index");

	lua_pop (L, 1);
}
