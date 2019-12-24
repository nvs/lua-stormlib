#include "common.h"
#include "mpq.h"
#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>

/**
 * `stormlib.open (path [, mode])`
 *
 * This function opens the MPQ archive specified by `path` (`string`), with
 * the specified `mode` (`string`).
 *
 * The `mode` can be any of the following, and must match exactly:
 *
 * - `"r"`: Read mode (the default).
 * - `"r+"`: Update mode.  Read and write functionality, preserving all
 *   existing data.
 * - `"w+"`: Update mode.  Read and write functionality, all previous
 *   data is erased and the archive is recreated.  Archives created in this
 *   fashion will have both `(listfile)` and `(attributes)` support.
 *
 * In case of success, this function returns a new `Storm MPQ` object.
 * Otherwise, it returns `nil`, a `string` describing the error, and a
 * `number` indicating the error code.
 */
static int
storm_open (lua_State *L)
{
	static const char *const
	modes [] = {
		"r",
		"r+",
		"w+",
		NULL
	};

	const char *path = luaL_checkstring (L, 1);

	int index = luaL_checkoption (L, 2, "r", modes);
	const char *mode = modes [index];

	struct Storm_MPQ *mpq;
	const int truncate = *mode++ == 'w';
	DWORD flags = 0;

	if (!truncate && !*mode)
	{
		flags = STREAM_FLAG_READ_ONLY;
	}

	mpq = storm_mpq_initialize (L);

	/*
	 * We wanted an archive, but failed to open it.  This is okay if we are
	 * trying to create an archive, as `SFileCreateArchive ()` errors if one
	 * exists and can be opened.
	 */
	if (!SFileOpenArchive (path, 0, flags, &mpq->handle))
	{
		if (!truncate)
		{
			goto error;
		}
	}

	/* We have an archive that can be opened, and need to truncate. */
	else if (truncate)
	{
		TFileStream *file;

		if (!SFileCloseArchive (mpq->handle))
		{
			goto error;
		}

		file = FileStream_CreateFile (path, 0);

		if (!file)
		{
			goto error;
		}

		FileStream_Close (file);
	}

	/* We have an archive to read. */
	else
	{
		goto out;
	}

	if (!SFileCreateArchive (path,
		MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES,
		HASH_TABLE_SIZE_MIN, &mpq->handle))
	{
		goto error;
	}

out:
	return 1;

error:
	return storm_result (L, 0);
}

static const luaL_Reg
storm_functions [] =
{
	{ "open", storm_open },
	{ NULL, NULL }
};

extern int
luaopen_stormlib (lua_State *L)
{
	luaL_newlib (L, storm_functions);

	return 1;
}

