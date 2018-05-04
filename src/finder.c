#include "finder.h"
#include "common.h"
#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>

#define STORM_FINDER_METATABLE "Storm Finder"

/**
 * `finder:__gc ()`
 *
 * Returns a `boolean` indicating that the MPQ finder was successfully
 * closed.
 *
 * In case of error, returns `nil`, a `string` describing the error, and
 * a `number` indicating the error code.
 */
static int
finder_close (lua_State *L)
{
	struct Storm_Finder *finder = storm_finder_access (L, 1);
	int status = 0;

	if (!finder->handle)
	{
		SetLastError (ERROR_INVALID_HANDLE);
	}
	else
	{
		status = SFileFindClose (finder->handle);
		finder->handle = NULL;
	}

	return storm_result (L, status);
}

/**
 * `finder:__tostring ()`
 *
 * Returns a `string` representation of the `Storm Finder` object,
 * indicating whether it is closed.
 */
static int
finder_to_string (lua_State *L)
{
	const struct Storm_Finder *finder = storm_finder_access (L, 1);
	const char *text;

	if (!finder->handle)
	{
		text = "%s (%p) (Closed)";
	}
	else
	{
		text = "%s (%p)";
	}

	lua_pushfstring (L, text, STORM_FINDER_METATABLE, finder);

	return 1;
}

static const luaL_Reg
finder_methods [] =
{
	{ "__tostring", finder_to_string },
	{ "__gc", finder_close },
	{ NULL, NULL }
};

extern struct Storm_Finder
*storm_finder_initialize (lua_State *L)
{
	struct Storm_Finder *finder = lua_newuserdata (L, sizeof (*finder));

	finder->handle = NULL;

	if (luaL_newmetatable (L, STORM_FINDER_METATABLE))
	{
		luaL_setfuncs (L, finder_methods, 0);

		lua_pushvalue (L, -1);
		lua_setfield (L, -2, "__index");
	}

	lua_setmetatable (L, -2);

	return finder;
}

extern struct Storm_Finder
*storm_finder_access (lua_State *L, int index)
{
	return luaL_checkudata (L, index, STORM_FINDER_METATABLE);
}
