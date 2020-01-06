#include "finder.h"
#include "common.h"
#include "handles.h"
#include "mpq.h"
#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>
#include <stddef.h>

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
		storm_handles_remove_finder (L, finder);
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
	const char *text = finder->handle ? "%s (%p)" : "%s (%p) (Closed)";

	lua_pushfstring (L, text, STORM_FINDER_METATABLE, finder);
	return 1;
}

static int
finder_iterator (lua_State *L)
{
	struct Storm_Finder *finder =
		storm_finder_access (L, lua_upvalueindex (1));
	const char *pattern = luaL_optstring (L, lua_upvalueindex (2), NULL);
	const int plain = lua_toboolean (L, lua_upvalueindex (3));

	SFILE_FIND_DATA data;
	int status;
	int error = ERROR_SUCCESS;
	int results = 0;

	while (true)
	{
		if (!finder->handle)
		{
			finder->handle = SFileFindFirstFile (
				finder->mpq->handle, "*", &data, NULL);
			error = GetLastError ();
			status = !!finder;

			if (status)
			{
				storm_handles_add_finder (L, lua_upvalueindex (1));
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

			status = !lua_isnil (L, -1);
			lua_pop (L, 1);
		}

		if (status)
		{
			lua_pushstring (L, data.cFileName);
			results = 1;
			break;
		}
	}

	if (!status)
	{
		lua_settop (L, 0);
		lua_pushvalue (L, lua_upvalueindex (1));
		finder_close (L);
	}

	if (status || error == ERROR_NO_MORE_FILES)
	{
		return results;
	}

	SetLastError (error);
	results = storm_result (L, 0);

error:
	return luaL_error (L, "%s", lua_tostring (L, -results + 1));
}

static const luaL_Reg
finder_methods [] =
{
	{ "__tostring", finder_to_string },
	{ "__gc", finder_close },
	{ NULL, NULL }
};

static void
finder_metatable (lua_State *L)
{
	if (luaL_newmetatable (L, STORM_FINDER_METATABLE))
	{
		luaL_setfuncs (L, finder_methods, 0);
		lua_pushvalue (L, -1);
		lua_setfield (L, -2, "__index");
	}

	lua_setmetatable (L, -2);
}

extern int
storm_finder_initialize (
	lua_State *L,
	const struct Storm_MPQ *mpq,
	const char *pattern,
	const int plain)
{
	struct Storm_Finder *finder = lua_newuserdata (L, sizeof (*finder));
	finder->handle = NULL;
	finder->mpq = mpq;

	finder_metatable (L);

	lua_pushstring (L, pattern);
	lua_pushboolean (L, plain);
	lua_pushcclosure (L, finder_iterator, 3);
	return 1;
}

extern struct Storm_Finder *
storm_finder_access (
	lua_State *L,
	int index)
{
	return luaL_checkudata (L, index, STORM_FINDER_METATABLE);
}
