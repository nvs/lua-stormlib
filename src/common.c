#include "common.h"
#include <StormLib.h>
#include <StormPort.h>
#include <lua.h>

extern int
storm_result (lua_State *L, const int status)
{
	const DWORD error = GetLastError ();

	if (status || error == ERROR_SUCCESS)
	{
		lua_pushboolean (L, status);

		return 1;
	}

	lua_pushnil (L);
	lua_pushstring (L, strerror (error));
	lua_pushinteger (L, (lua_Integer) error);

	return 3;
}
