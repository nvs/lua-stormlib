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
 *
 * In case of success, this function returns a new `Storm MPQ` object.
 * Otherwise, it returns `nil`, a `string` describing the error, and a
 * `number` indicating the error code.
 */
static int
storm_open (lua_State *L)
{
	const char *path = luaL_checkstring (L, 1);

	size_t length;
	const char *mode = luaL_optlstring (L, 2, "r", &length);

	struct Storm_MPQ *mpq;
	DWORD flags = 0;

	if (length > 2 || *mode++ != 'r' || (*mode && *mode != '+'))
	{
		return luaL_argerror (L, 2, "invalid mode");
	}

	if (!*mode)
	{
		flags = STREAM_FLAG_READ_ONLY;
	}

	mpq = storm_mpq_initialize (L);

	if (!SFileOpenArchive (path, 0, flags, &mpq->handle))
	{
		goto error;
	}

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

