#include "mpq.h"
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>
#include <stddef.h>

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
	static const int
	modes [] = {
		READ,
		UPDATE,
		WRITE
	};

	static const char *const
	modes_options [] = {
		"r",
		"r+",
		"w+",
		NULL
	};

	const char *path = luaL_checkstring (L, 1);
	const int index = luaL_checkoption (L, 2, "r", modes_options);
	const enum modes mode = modes [index];

	lua_settop (L, 0);
	return storm_mpq_initialize (L, path, mode);
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

