#include "handles.h"
#include "file.h"
#include "finder.h"
#include "mpq.h"
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>

#define STORM_HANDLES_METATABLE "Storm Handles"

extern void
storm_handles_initialize (
	lua_State *L,
	const struct Storm_MPQ *mpq)
{
	lua_newtable (L);

	if (luaL_newmetatable (L, STORM_HANDLES_METATABLE))
	{
		lua_pushliteral (L, "v");
		lua_setfield (L, -2, "__mode");
	}

	lua_setmetatable (L, -2);
	lua_rawsetp (L, LUA_REGISTRYINDEX, mpq->handle);
}

extern void
storm_handles_close (
	lua_State *L,
	const struct Storm_MPQ *mpq)
{
	lua_rawgetp (L, LUA_REGISTRYINDEX, mpq->handle);
	lua_pushnil (L);

	while (lua_next (L, -2))
	{
		lua_getmetatable (L, -1);
		lua_getfield (L, -1, "__gc");
		lua_remove (L, -2);
		lua_insert (L, -2);
		lua_call (L, 1, 0);
	}

	lua_pop (L, 1);
	lua_pushnil (L);
	lua_rawsetp (L, LUA_REGISTRYINDEX, mpq->handle);
}

static void
handles_iterate (
	lua_State* L,
	HANDLE *mpq,
	const Storm_Handles_Callback callback)
{
	lua_rawgetp (L, LUA_REGISTRYINDEX, mpq);
	lua_pushnil (L);

	while (lua_next (L, -2))
	{
		lua_getmetatable (L, -1);

		if (lua_rawequal (L, -1, -5))
		{
			callback (L, lua_touserdata (L, -2));
		}

		lua_pop (L, 2);
	}

	lua_pop (L, 1);
}

extern void
storm_handles_iterate_files (
	lua_State* L,
	const struct Storm_MPQ *mpq,
	const Storm_Handles_Callback callback)
{
	luaL_getmetatable (L, STORM_FILE_METATABLE);
	handles_iterate (L, mpq->handle, callback);
}

extern void
storm_handles_iterate_finders (
	lua_State* L,
	const struct Storm_MPQ *mpq,
	const Storm_Handles_Callback callback)
{
	luaL_getmetatable (L, STORM_FINDER_METATABLE);
	handles_iterate (L, mpq->handle, callback);
}

static void
handles_add (
	lua_State *L,
	int index,
	HANDLE *mpq,
	HANDLE *handle)
{
	lua_pushvalue (L, index);
	lua_rawgetp (L, LUA_REGISTRYINDEX, mpq);
	lua_insert (L, -2);
	lua_rawsetp (L, -2, handle);
	lua_pop (L, 1);
}

extern void
storm_handles_add_file (
	lua_State *L,
	int index)
{
	const struct Storm_File *file = storm_file_access (L, index);
	handles_add (L, index, file->mpq->handle, file->handle);
}

extern void
storm_handles_add_finder (
	lua_State *L,
	int index)
{
	const struct Storm_Finder *finder = storm_finder_access (L, index);
	handles_add (L, index, finder->mpq->handle, finder->handle);
}

static void
handles_remove (
	lua_State *L,
	HANDLE *mpq,
	HANDLE *handle)
{
	lua_rawgetp (L, LUA_REGISTRYINDEX, mpq);
	lua_pushnil (L);
	lua_rawsetp (L, -2, handle);
	lua_pop (L, 1);
}

extern void
storm_handles_remove_file (
	lua_State* L,
	const struct Storm_File* file)
{
	handles_remove (L, file->mpq->handle, file->handle);
}

extern void
storm_handles_remove_finder (
	lua_State* L,
	const struct Storm_Finder* finder)
{
	handles_remove (L, finder->mpq->handle, finder->handle);
}
