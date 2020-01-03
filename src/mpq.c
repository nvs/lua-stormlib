#include "mpq.h"
#include "common.h"
#include "file.h"
#include "finder.h"
#include "handles.h"
#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>
#include <stdio.h>
#include <string.h>

#define STORM_MPQ_METATABLE "Storm MPQ"

static int
mpq_increase_limit (const struct Storm_MPQ *mpq)
{
	DWORD count;
	DWORD limit;

	/* The count may be off unless we explicitly flush. */
	if (!SFileFlushArchive (mpq->handle))
	{
		return 0;
	}

	if (!SFileGetFileInfo (mpq->handle,
		SFileMpqNumberOfFiles, &count, sizeof (count), 0))
	{
		return 0;
	}

	if (!SFileGetFileInfo (mpq->handle,
		SFileMpqMaxFileCount, &limit, sizeof (limit), 0))
	{
		return 0;
	}

	if (count == limit && !SFileSetMaxFileCount (mpq->handle, limit + 1))
	{
		return 0;
	}

	return 1;
}

static int
mpq_files_iterator (lua_State *L)
{
	struct Storm_Finder *finder =
		storm_finder_access (L, lua_upvalueindex (4));
	const char *pattern = luaL_optstring (L, lua_upvalueindex (2), NULL);
	const int plain = lua_toboolean (L, lua_upvalueindex (3));

	SFILE_FIND_DATA data;
	int status;
	int results;

	while (true)
	{
		if (!finder->handle)
		{
			const struct Storm_MPQ *mpq =
				storm_mpq_access (L, lua_upvalueindex (1));

			finder->handle = SFileFindFirstFile (
				mpq->handle, "*", &data, NULL);
			status = !!finder->handle;

			if (status)
			{
				storm_handles_add_finder (L, lua_upvalueindex (4));
			}
		}
		else
		{
			status = SFileFindNextFile (finder->handle, &data);
		}

		if (!status)
		{
			break;
		}

		if (pattern)
		{
			lua_getglobal (L, "string");
			lua_getfield (L, -1, "find");
			lua_remove (L, -2);

			lua_pushstring (L, data.cFileName);
			lua_pushstring (L, pattern);
			lua_pushnil (L);
			lua_pushboolean (L, plain);
			lua_call (L, 4, 1);

			if (lua_isnil (L, -1))
			{
				status = 0;
			}
			else
			{
				lua_remove (L, -1);
			}
		}

		if (status)
		{
			lua_pushstring (L, data.cFileName);
			return 1;
		}
	}

	if (GetLastError () == ERROR_NO_MORE_FILES)
	{
		return 0;
	}

	results = storm_result (L, 0);
	return luaL_error (L, "%s", lua_tostring (L, -results + 1));
}

/**
 * `mpq:files ([pattern [, plain]])`
 *
 * Returns an iterator `function` that, each time it is called, returns the
 * next file name (`string`) that matches `pattern` (`string`) (which is a
 * Lua pattern).  If `plain` (`boolean`) is specified, then pattern matching
 * is disabled and a plain text search is performed.  The default behavior,
 * should `pattern` be absent, is to return all files.
 *
 * In case of errors this function raises the error, instead of returning an
 * error code.
 */
static int
mpq_files (lua_State *L)
{
	struct Storm_MPQ *mpq = storm_mpq_access (L, 1);

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	if (!lua_isnoneornil (L, 2))
	{
		luaL_checkstring (L, 2);
	}

	lua_settop (L, 3);

	struct Storm_Finder *finder = storm_finder_initialize (L);
	finder->mpq = mpq;

	lua_pushcclosure (L, mpq_files_iterator, 4);
	return 1;

error:
	return storm_result (L, 0);
}

/**
 * `mpq:open (name [, mode [, size]])`
 *
 * This function opens the file specified by `name` (`string`) within the
 * `mpq` archive, with the specified `mode` (`string`), and returns a new
 * Storm File object.
 *
 * The `mode` can be any of the following, and must match exactly:
 *
 * - `"r"`: Read mode (the default).
 * - `"w"`: Write mode.  Truncates existing files.  Writes behave like
 *   append mode, in that they are forced to the then current end of file.
 *
 * If `mode` is `"w"`, then an additional `size` (`number`) argument must be
 * provided, representing the size of the file.  The subsequent amount of
 * data written must equal this value.
 *
 * Additionally, `"b"` is accepted at the end of the mode, representing
 * binary mode.  However, it serves no actual purpose.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_open (lua_State *L)
{
	static const char *const
	modes [] = {
		"r",
		"rb",
		"w",
		"wb",
		NULL
	};

	struct Storm_MPQ *mpq = storm_mpq_access (L, 1);

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	const char *name = luaL_checkstring (L, 2);
	const int index = luaL_checkoption (L, 3, "r", modes);
	const char *mode = modes [index];
	lua_Integer size;

	if (*mode == 'r')
	{
		size = -1;
	}
	else
	{
		size = luaL_checkinteger (L, 4);
		luaL_argcheck (L, size >= 0, 4, "size cannot be negative");

		if (!mpq_increase_limit (mpq))
		{
			goto error;
		}
	}

	lua_settop (L, 0);
	return storm_file_initialize (L, mpq, name, size);

error:
	return storm_result (L, 0);
}

/**
 * `mpq:remove (name)`
 *
 * Returns a `boolean` indicating the the file specified by `name`
 * (`string`) was successfully removed from the `mpq` archive.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_remove (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *path = luaL_checkstring (L, 2);
	int status = 0;

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto out;
	}

	status = SFileRemoveFile (mpq->handle, path, 0);

out:
	return storm_result (L, status);
}

/**
 * `mpq:rename (old, new)`
 *
 * Returns a `boolean` indicating that the file specified  by `old`
 * (`string`) within the `mpq` archive was successfully renamed to `new`
 * (`string`).
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_rename (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *old = luaL_checkstring (L, 2);
	const char *new = luaL_checkstring (L, 3);
	int status = 0;

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto out;
	}

	status = SFileRenameFile (mpq->handle, old, new);

out:
	return storm_result (L, status);
}

/**
 * `mpq:compact ()`
 *
 * Returns a `boolean` indicating that the `mpq` archive was successfully
 * rebuilt.  This effectively defragments the archive, removing all gaps
 * that have been created by adding, replacing, renaming, or deleting files.
 *
 * Note that this has the potential to be a costly operation on some
 * archives.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_compact (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	int status = 0;

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto out;
	}

	status = SFileCompactArchive (mpq->handle, NULL, 0);

out:
	return storm_result (L, status);
}

/**
 * `mpq:close ()`
 *
 * Returns a `boolean` indicating that the `mpq` archive, along with any of
 * its open files, was successfully closed.  Note that archives are
 * automatically closed when their handles are garbage collected.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_close (lua_State *L)
{
	struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	int status = 0;

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
	}
	else
	{
		storm_handles_close (L, mpq);
		status = SFileCloseArchive (mpq->handle);
		mpq->handle = NULL;
	}

	return storm_result (L, status);
}

/**
 * `mpq:__tostring ()`
 *
 * Returns a `string` representation of the `mpq` archive, indicating
 * whether it is closed, open for writing, or open for reading.
 */
static int
mpq_to_string (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *text;

	if (!mpq->handle)
	{
		text = "%s (%p) (Closed)";
	}
	else
	{
		text = "%s (%p)";
	}

	lua_pushfstring (L, text, STORM_MPQ_METATABLE, mpq);
	return 1;
}

static const luaL_Reg
mpq_methods [] =
{
	{ "files", mpq_files },
	{ "open", mpq_open },
	{ "rename", mpq_rename },
	{ "remove", mpq_remove },
	{ "compact", mpq_compact },
	{ "close", mpq_close },
	{ "__tostring", mpq_to_string },
	{ "__gc", mpq_close },
	{ NULL, NULL }
};

static void
mpq_metatable (lua_State *L)
{
	if (luaL_newmetatable (L, STORM_MPQ_METATABLE))
	{
		luaL_setfuncs (L, mpq_methods, 0);
		lua_pushvalue (L, -1);
		lua_setfield (L, -2, "__index");
	}

	lua_setmetatable (L, -2);
}

extern int
storm_mpq_initialize (
	lua_State *L,
	const char *path,
	const enum modes mode)
{
	HANDLE handle;
	const DWORD flags = mode == READ ? STREAM_FLAG_READ_ONLY : 0;
	const int status = SFileOpenArchive (path, 0, flags, &handle);

	switch (mode)
	{
		case READ:
		case UPDATE:
		{
			if (!status)
			{
				goto error;
			}

			break;
		}

		case WRITE:
		{
			if (status)
			{
				if (!SFileCloseArchive (handle))
				{
					goto error;
				}

				if (remove (path) == -1)
				{
					goto error;
				}
			}

			if (!SFileCreateArchive (
				path, MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES,
				HASH_TABLE_SIZE_MIN, &handle))
			{
				goto error;
			}

			break;
		}
	}


	struct Storm_MPQ *mpq = lua_newuserdata (L, sizeof (*mpq));
	mpq->handle = handle;

	mpq_metatable (L);
	storm_handles_initialize (L, mpq);

	return 1;

error:
	return storm_result (L, 0);
}

extern struct Storm_MPQ
*storm_mpq_access (lua_State *L, int index)
{
	return luaL_checkudata (L, index, STORM_MPQ_METATABLE);
}
