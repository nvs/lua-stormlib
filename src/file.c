#include "file.h"
#include "common.h"
#include "handles.h"
#include "mpq.h"
#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>
#include <luaconf.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/**
 * `file:seek ([whence [, offset]])`
 *
 * Sets and gets the file position, measured from the beginning of the file,
 * to the position given by `offset` (`number`) plus a base specified by
 * `whence` (`string`), as follows:
 *
 * - `"set"`: Base is position `0` (i.e. the beginning of the file).
 * - `"cur"`: Base is the current position.  For writable files, this is the
 *   last written position.
 * - `"end"`: Base is the end of the file.
 *
 * In case of success, this function returns the final file position,
 * measured in bytes from the beginning of the file.  Otherwise, it returns
 * `nil`, a `string` describing the error, and a `number` indicating the
 * error code.
 *
 * The default value for `whence` is `"cur"`, and for offset is `0`.
 * Therefore, the call `file:seek ()` returns the current file position,
 * without changing it; the call `file:seek ('set')` sets the position to
 * the beginning of the file (and returns `0`); and the call `file:seek
 * ('end')` sets the position to the end of the file, and returns its size.
 *
 * Note that behavior for writable files is quite limited, and does not
 * actually adjust the file position.  Only `"cur"` and `"end`" are
 * supported, respectively returning the last written position and end of
 * the file.  Additionally, an `offset`, if provided, must equal `0`.  All
 * other usages will return `nil`.
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
	TMPQFile *handle = file->handle;
	DWORD position = 0;

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	if (file->is_writable)
	{
		luaL_argcheck (L, offset == 0, 3,
			"offset must be `0` for writable files");

		switch (mode)
		{
			case FILE_BEGIN:
			{
				luaL_argerror (L, 3, "cannot use 'set' for writable files");
				break;
			}

			case FILE_CURRENT:
			{
				position = handle->dwFilePos;
				break;
			}

			case FILE_END:
			{
				position = handle->pFileEntry->dwFileSize;
				break;
			}
		}
	}
	else
	{
		position = SFileSetFilePointer (handle, offset, NULL, mode);

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
read_line (
	lua_State *L,
	const struct Storm_File *file,
	int chop)
{
	luaL_Buffer line;

	char character = '\0';
	int status = 1;
	int error;

	luaL_buffinit (L, &line);

	while (status && character != '\n')
	{
		char *buffer = luaL_prepbuffer (&line);
		int index = 0;

		while (index < LUAL_BUFFERSIZE)
		{
			status = SFileReadFile (
				file->handle, &character, 1, NULL, NULL);
			error = GetLastError ();

			if (!status || character == '\n')
			{
				break;
			}

			buffer [index++] = character;
		}

		luaL_addsize (&line, index);
	}

	if (!chop && character == '\n')
	{
		luaL_addchar (&line, character);
	}

	luaL_pushresult (&line);
	SetLastError (error);
	return status;
}

static int
read_characters (
	lua_State *L,
	const struct Storm_File *file,
	lua_Integer count)
{
	luaL_Buffer characters;
	luaL_buffinit (L, &characters);

	DWORD bytes_to_read;
	DWORD bytes_read;
	int status = 1;
	int error = ERROR_SUCCESS;

	while (count > 0 && status)
	{
		bytes_to_read = count > LUAL_BUFFERSIZE ? LUAL_BUFFERSIZE : count;
		count -= bytes_to_read;
		char *buffer = luaL_prepbuffsize (&characters, bytes_to_read);

		status = SFileReadFile (
			file->handle, buffer, bytes_to_read, &bytes_read, NULL);
		error = GetLastError ();

		luaL_addsize (&characters, bytes_read);
	}

	luaL_pushresult (&characters);
	SetLastError (error);
	return status;
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

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	if (file->is_writable)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	DWORD size = SFileGetFileSize (file->handle, NULL);

	if (size == SFILE_INVALID_SIZE)
	{
		goto error;
	}

	int index = 1;
	int arguments = lua_gettop (L) - index++;

	/* By default, read a line (with no line break). */
	if (arguments == 0)
	{
		arguments++;
		lua_pushliteral (L, "l");
	}

	/* Ensure stack space for all results and the buffer. */
	luaL_checkstack (L, arguments + LUA_MINSTACK, "too many arguments");

	int status = 1;

	for (; arguments-- && status; index++)
	{
		if (lua_type (L, index) == LUA_TNUMBER)
		{
			const lua_Integer count = luaL_checkinteger (L, index);
			status = read_characters (L, file, count);

			continue;
		}

		const char *format = luaL_checkstring (L, index);

		if (*format == '*')
		{
			format++;
		}

		switch (*format)
		{
			case 'l':
			{
				status = read_line (L, file, 1);
				break;
			}

			case 'L':
			{
				status = read_line (L, file, 0);
				break;
			}

			case 'a':
			{
				read_characters (L, file, size);
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
		if (GetLastError () != ERROR_HANDLE_EOF)
		{
			goto error;
		}

		/*
		 * Note that compat-5.3 simply defines `lua_rawlen` as a macro to
		 * `lua_objlen` for Lua 5.1, despite the behavior not being
		 * analogous for `number`.  Ensure that only `string` is at the top
		 * of the stack to avoid this issue.
		 */
		if (lua_rawlen (L, -1) == 0)
		{
			lua_pop (L, 1);
			lua_pushnil (L);
		}
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
	int results = 0;

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	lua_settop (L, 0);
	lua_pushvalue (L, lua_upvalueindex (1));

	const int arguments = (int) lua_tointeger (L, lua_upvalueindex (2));
	luaL_checkstack (L, arguments, "too many arguments");

	for (int index = 1; index <= arguments; index++)
	{
		lua_pushvalue (L, lua_upvalueindex (2 + index));
	}

	results = file_read (L);

	/* If the first result is not `nil`, return all results. */
	if (lua_toboolean (L, -results))
	{
		return results;
	}

error:
	/* A lack of results implies we did not attempt to read the file. */
	if (results == 0)
	{
		results = storm_result (L, 0);
	}

	/* Is there error information? */
	if (results > 1)
	{
		return luaL_error (L, "%s", lua_tostring (L, -results + 1));
	}

	/* Otherwise, this should only mean EOF. */
	return 0;
}

/*
 * This plus the number of upvalues used must be less than the maximum
 * number of upvalues to a C function (i.e. `255`).
 */
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

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	const int arguments = lua_gettop (L) - 1;

	luaL_argcheck (L, arguments <= LINES_MAXIMUM_ARGUMENTS,
		LINES_MAXIMUM_ARGUMENTS + 1, "too many arguments");

	lua_pushinteger (L, arguments);
	lua_insert (L, 2);
	lua_pushcclosure (L, lines_iterator, 2 + arguments);
	return 1;

error:
	return storm_result (L, 0);
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

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	if (!file->is_writable)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	int index = 1;
	int arguments = lua_gettop (L) - index++;

	for (; arguments--; index++)
	{
		size_t size;
		const char *text = luaL_checklstring (L, index, &size);

		if (!SFileWriteFile (
			file->handle, text, size, MPQ_COMPRESSION_ZLIB))
		{
			goto error;
		}

	}

	lua_settop (L, 1);
	return 1;

error:
	return storm_result (L, 0);
}

/**
 * `file:setvbuf ()`
 *
 * Returns `true`.  The buffering mode when writing cannot be altered.
 *
 * This function exists to maintain consistency with Lua's I/O library.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
file_setvbuf (lua_State *L)
{
	struct Storm_File *file = storm_file_access (L, 1);
	int status = 1;

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		status = 0;
	}

	return storm_result (L, status);
}

/**
 * `file:flush ()`
 *
 * Returns `true`.  Data is automatically flushed to disk during write.
 *
 * This function exists to maintain consistency with Lua's I/O library.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
file_flush (lua_State *L)
{
	struct Storm_File *file = storm_file_access (L, 1);
	int status = 1;

	if (!file->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		status = 0;
	}

	return storm_result (L, status);
}

/**
 * `file:close ()`
 *
 * Returns a `boolean` indicating that the file was successfully closed.
 * Note that files are automatically closed when their handles are garbage
 * collected or when the archive they belong to is closed.
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
	else
	{
		storm_handles_remove_file (L, file);

		if (file->is_writable)
		{
			status = SFileFinishFile (file->handle);
		}
		else
		{
			status = SFileCloseFile (file->handle);
		}

		file->handle = NULL;
		file->mpq = NULL;
		free (file->name);
		file->name = NULL;
	}

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
	{ "seek", file_seek },
	{ "read", file_read },
	{ "lines", file_lines },
	{ "write", file_write },
	{ "setvbuf", file_setvbuf },
	{ "flush", file_flush },
	{ "close", file_close },
	{ "__tostring", file_to_string },
	{ "__gc", file_close },
	{ NULL, NULL }
};

static void
file_metatable (
	lua_State *L)
{
	if (luaL_newmetatable (L, STORM_FILE_METATABLE))
	{
		luaL_setfuncs (L, file_methods, 0);
		lua_pushvalue (L, -1);
		lua_setfield (L, -2, "__index");
	}

	lua_setmetatable (L, -2);
}

extern int
storm_file_initialize (
	lua_State *L,
	struct Storm_MPQ *mpq,
	const char *name,
	const lua_Integer size)
{
	HANDLE handle;
	const int is_writable = size >= 0;

	if (is_writable)
	{
		if (!SFileCreateFile (mpq->handle, name, 0, size, 0,
			MPQ_FILE_REPLACEEXISTING | MPQ_FILE_COMPRESS, &handle))
		{
			if (handle)
			{
				SFileFinishFile (handle);
				handle = NULL;
			}

			goto error;
		}
	}
	else if (!SFileOpenFileEx (mpq->handle, name, 0, &handle))
	{
		goto error;
	}

	struct Storm_File *file = lua_newuserdata (L, sizeof (*file));
	file->handle = handle;
	file->name = malloc (sizeof (*file->name) * strlen (name) + 1);
	strcpy (file->name, name);
	file->mpq = mpq;
	file->is_writable = is_writable;

	file_metatable (L);
	storm_handles_add_file (L, -1);

	return 1;

error:
	return storm_result (L, 0);
}

extern struct Storm_File *
storm_file_access (
	lua_State *L,
	int index)
{
	return luaL_checkudata (L, index, STORM_FILE_METATABLE);
}
