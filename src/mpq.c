#include "mpq.h"
#include "common.h"
#include "file.h"
#include "finder.h"
#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>
#include <string.h>

#define STORM_MPQ_METATABLE "Storm MPQ"

extern struct Storm_MPQ
*storm_mpq_initialize (lua_State *L)
{
	struct Storm_MPQ *mpq = lua_newuserdata (L, sizeof (*mpq));

	mpq->handle = NULL;

	luaL_setmetatable (L, STORM_MPQ_METATABLE);

	return mpq;
}

extern struct Storm_MPQ
*storm_mpq_access (lua_State *L, int index)
{
	return luaL_checkudata (L, index, STORM_MPQ_METATABLE);
}

static int
mpq_increase_limit (const struct Storm_MPQ *mpq)
{
	DWORD count;
	DWORD limit;

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

/**
 * `mpq:count ()`
 *
 * Returns the `number` of files present with the `mpq` archive.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_count (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);

	DWORD count;

	if (!SFileGetFileInfo (mpq->handle,
		SFileMpqNumberOfFiles, &count, sizeof (count), 0))
	{
		return storm_result (L, 0);
	}

	lua_pushinteger (L, (lua_Integer) count);

	return 1;
}

/**
 * `mpq:limit ()`
 *
 * Returns the maximum `number` of files that the `mpq` archive can contain.
 * This typically will be a value that is a power of two.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_limit (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);

	DWORD limit;

	if (!SFileGetFileInfo (mpq->handle,
		SFileMpqMaxFileCount, &limit, sizeof (limit), 0))
	{
		return storm_result (L, 0);
	}

	lua_pushinteger (L, (lua_Integer) limit);

	return 1;
}

/**
 * `mpq:has (name)`
 *
 * Returns a `boolean` indicating whether the `mpq` archive contains the
 * file specified by `name (string)`.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_has (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *name = luaL_checkstring (L, 2);

	int status = SFileHasFile (mpq->handle, name);

	if (!status && GetLastError () == ERROR_FILE_NOT_FOUND)
	{
		SetLastError (ERROR_SUCCESS);
	}

	return storm_result (L, status);
}

static int
mpq_list_iterator (lua_State *L)
{
	struct Storm_Finder *finder =
		storm_finder_access (L, lua_upvalueindex (1));

	SFILE_FIND_DATA data;
	int status;
	int results;

	if (!finder->handle)
	{
		const struct Storm_MPQ *mpq =
			storm_mpq_access (L, lua_upvalueindex (2));
		const char *mask = lua_tostring (L, lua_upvalueindex (3));

		finder->handle = SFileFindFirstFile (
			mpq->handle, mask, &data, NULL);
		status = !!finder->handle;
	}
	else
	{
		status = SFileFindNextFile (finder->handle, &data);
	}

	if (status)
	{
		lua_pushstring (L, data.cFileName);
		return 1;
	}

	if (GetLastError () == ERROR_NO_MORE_FILES)
	{
		return 0;
	}

	results = storm_result (L, 0);
	return luaL_error (L, "%s", lua_tostring (L, -results + 1));
}

/**
 * `mpq:list ([mask])`
 *
 * Returns an iterator `function` that, each time it is called, returns the
 * next file name (a `string`) that matches `mask (string)`.  The default
 * `mask` value is `"*"`, which will return all files.
 *
 * A `mask` supports two control charcters, neither of which can be escaped:
 *
 * `"*"`: This will match zero or more characters.
 * `"?"`: This will match any single character.
 *
 * In case of errors this function raises the error, instead of returning an
 * error code.
 */
static int
mpq_list (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		return storm_result (L, 0);
	}

	if (lua_isnoneornil (L, 2))
	{
		lua_pushliteral (L, "*");
	}
	else
	{
		luaL_checkstring (L, 2);
	}

	storm_finder_initialize (L);
	lua_insert (L, 1);

	lua_pushcclosure (L, mpq_list_iterator, 3);

	return 1;
}

/**
 * `mpq:open (name [, mode [, size]])`
 *
 * This function opens the file specified by `name (string)` within the the
 * `mpq` archive, with the specified `mode (string)`, and returns a `Storm
 * File` object.
 *
 * The `mode` can be any of the following, and must match exactly:
 *
 * - `"r"`: Read mode (the default).
 * - `"w"`: Write mode.  Truncates existing files.  Writes behave like
 *   append mode, in that they are forced to the then current end of file.
 *
 * If `mode` is `"w"`, then an additional `size (number)` argument must be
 * provided, representing the size of the file.  The subsequent amount of
 * data written must equal this value.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_open (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *name = luaL_checkstring (L, 2);

	size_t length;
	const char *mode = luaL_optlstring (L, 3, "r", &length);

	struct Storm_File *file;

	if (length > 1 || !strchr ("rw", *mode))
	{
		return luaL_argerror (L, 3, "invalid mode");
	}

	file = storm_file_initialize (L);

	if (*mode == 'w')
	{
		DWORD size = (DWORD) luaL_checkinteger (L, 4);

		if (!mpq_increase_limit (mpq))
		{
			goto error;
		}

		if (!SFileCreateFile (mpq->handle, name, 0, size, 0,
			/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
			MPQ_FILE_REPLACEEXISTING | MPQ_FILE_COMPRESS, &file->handle))
		{
			goto error;
		}
	}
	else
	{
		if (!SFileOpenFileEx (mpq->handle, name, 0, &file->handle))
		{
			goto error;
		}
	}

	return 1;

error:
	return storm_result (L, 0);
}

/**
 * `mpq:add (path [, name])`
 *
 * Returns a `boolean` indicating that the file specified by `path (string)`
 * was successfully added to the `mpq` archive as `name (string)`.  The
 * default value for `name` is `path`.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_add (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *path = luaL_checkstring (L, 2);
	const char *name = luaL_optstring (L, 3, path);

	int status = 0;

	if (!mpq_increase_limit (mpq))
	{
		goto error;
	}

	if (!SFileAddFileEx (mpq->handle, path, name,
		/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
		MPQ_FILE_REPLACEEXISTING | MPQ_FILE_COMPRESS,
		MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_ZLIB))
	{
		goto error;
	}

	/* The count may be off by one unless we explicitly flush.  On the plus
	 * side, this makes it more difficult to corrupt the archive on error.
	 */
	if (!SFileFlushArchive (mpq->handle))
	{
		goto error;
	}

	status = 1;

error:
	return storm_result (L, status);
}

/**
 * `mpq:extract (name, path)`
 *
 * Returns a `boolean` indicating that the file specified by `name (string)`
 * was successfully extracted from the `mpq` archive to `path (string)`.
 * Note that this function will not create directory paths.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_extract (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *name = luaL_checkstring (L, 2);
	const char *path = luaL_checkstring (L, 3);

	int status = SFileExtractFile (mpq->handle, name, path, 0);

	return storm_result (L, status);
}

/**
 * `mpq:remove (name)`
 *
 * Returns a `boolean` indicating the the file specified by `name (string)`
 * was successfully removed from the `mpq` archive.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_remove (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *path = luaL_checkstring (L, 2);

	int status = SFileRemoveFile (mpq->handle, path, 0);

	return storm_result (L, status);
}

/**
 * `mpq:rename (old, new)`
 *
 * Returns a `boolean` indicating that the file specified  by `old (string)`
 * with the `mpq` archive was successfully renamed to `new (string)`.
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

	int status = SFileRenameFile (mpq->handle, old, new);

	return storm_result (L, status);
}

/**
 * `mpq:compact ()`
 *
 * Returns a `boolean` indicating that the `mpq` archive was successfully
 * rebuilt.  This effectively defragments the archive, removing all gaps
 * that have been created by adding, replacing, renaming, or deleting files.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_compact (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	int status = SFileCompactArchive (mpq->handle, NULL, 0);

	return storm_result (L, status);
}

/**
 * `mpq:close ()`
 *
 * Returns a `boolean` indicating that the `mpq` archive was successfully
 * closed.
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
	{ "count", mpq_count },
	{ "limit", mpq_limit },
	{ "has", mpq_has },
	{ "list", mpq_list },
	{ "open", mpq_open },
	{ "add", mpq_add },
	{ "extract", mpq_extract },
	{ "rename", mpq_rename },
	{ "remove", mpq_remove },
	{ "compact", mpq_compact },
	{ "close", mpq_close },
	{ "__tostring", mpq_to_string },
	{ "__gc", mpq_close },
	{ NULL, NULL }
};

extern void
storm_mpq_metatable (lua_State *L)
{
	luaL_newmetatable (L, STORM_MPQ_METATABLE);
	luaL_setfuncs (L, mpq_methods, 0);

	lua_pushvalue (L, -1);
	lua_setfield (L, -2, "__index");

	lua_pop (L, 1);
}