#include "mpq.h"
#include "common.h"
#include "file.h"
#include "files.h"
#include "finder.h"
#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>
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

/**
 * `mpq:has (name)`
 *
 * Returns a `boolean` indicating whether the `mpq` archive contains the
 * file specified by `name` (`string`).
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
mpq_has (lua_State *L)
{
	const struct Storm_MPQ *mpq = storm_mpq_access (L, 1);
	const char *name = luaL_checkstring (L, 2);
	int status = 0;

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto out;
	}

	status = SFileHasFile (mpq->handle, name);

	if (!status && GetLastError () == ERROR_FILE_NOT_FOUND)
	{
		SetLastError (ERROR_SUCCESS);
	}

out:
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
 * next file name (`string`) that matches `mask` (`string`).  The default
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
		goto error;
	}

	if (lua_isnone (L, 2))
	{
		lua_pushnil (L);
	}

	if (lua_isnil (L, 2))
	{
		lua_pop (L, 1);
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

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto error;
	}

	file = storm_file_initialize (L);
	file->is_writable = *mode == 'w';

	if (file->is_writable)
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
			if (file->handle)
			{
				SFileFinishFile (file->handle);
				file->handle = 0;
			}

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

	storm_files_insert (L, mpq, -1);
	return 1;

error:
	return storm_result (L, 0);
}

/**
 * `mpq:add (path [, name])`
 *
 * Returns a `boolean` indicating that the file specified by `path`
 * (`string`) was successfully added to the `mpq` archive as `name`
 * (`string`).  The default value for `name` is `path`.
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

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto out;
	}

	if (!mpq_increase_limit (mpq))
	{
		goto out;
	}

	if (!SFileAddFileEx (mpq->handle, path, name,
		/* NOLINTNEXTLINE(hicpp-signed-bitwise) */
		MPQ_FILE_REPLACEEXISTING | MPQ_FILE_COMPRESS,
		MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_ZLIB))
	{
		goto out;
	}



	status = 1;

out:
	return storm_result (L, status);
}

/**
 * `mpq:extract (name, path)`
 *
 * Returns a `boolean` indicating that the file specified by `name`
 * (`string`) was successfully extracted from the `mpq` archive to `path`
 * (`string`).  Note that this function will not create directory paths.
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
	int status = 0;

	if (!mpq->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
		goto out;
	}

	status = SFileExtractFile (mpq->handle, name, path, 0);

out:
	return storm_result (L, status);
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
		storm_files_close (L, mpq);
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

extern struct Storm_MPQ
*storm_mpq_initialize (lua_State *L)
{
	struct Storm_MPQ *mpq = lua_newuserdata (L, sizeof (*mpq));

	mpq->handle = NULL;

	if (luaL_newmetatable (L, STORM_MPQ_METATABLE))
	{
		luaL_setfuncs (L, mpq_methods, 0);

		lua_pushvalue (L, -1);
		lua_setfield (L, -2, "__index");
	}

	lua_setmetatable (L, -2);
	storm_files_initialize (L, mpq);

	return mpq;
}

extern struct Storm_MPQ
*storm_mpq_access (lua_State *L, int index)
{
	return luaL_checkudata (L, index, STORM_MPQ_METATABLE);
}
