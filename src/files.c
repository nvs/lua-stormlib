#include "files.h"
#include "file.h"
#include "mpq.h"
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>

#define STORM_FILES_METATABLE "Storm Files"

extern void
storm_files_initialize (lua_State *L, const struct Storm_MPQ *mpq)
{
	lua_newtable (L);

	if (luaL_newmetatable (L, STORM_FILES_METATABLE))
	{
		lua_pushliteral (L, "v");
		lua_setfield (L, -2, "__mode");
	}

	lua_setmetatable (L, -2);
	lua_rawsetp (L, LUA_REGISTRYINDEX, (void *) mpq);
}

extern void
storm_files_insert (lua_State *L, const struct Storm_MPQ *mpq, int index)
{
	const struct Storm_File *file = storm_file_access (L, index);

	lua_pushvalue (L, index);
	lua_rawgetp (L, LUA_REGISTRYINDEX, (void *) mpq);
	lua_insert (L, -2);
	lua_rawsetp (L, -2, (void *) file);
	lua_pop (L, 1);
}

extern void
storm_files_close (lua_State *L, const struct Storm_MPQ *mpq)
{
	lua_rawgetp (L, LUA_REGISTRYINDEX, (void *) mpq);

	lua_pushnil (L);

	while (lua_next (L, -2))
	{
		lua_getmetatable (L, -1);
		lua_getfield (L, -1, "close");
		lua_remove (L, -2);
		lua_insert (L, -2);
		lua_call (L, 1, 0);
	}

	lua_pop (L, 1);

	lua_pushnil (L);
	lua_rawsetp (L, LUA_REGISTRYINDEX, (void *) mpq);
}
