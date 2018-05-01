#ifndef STORM_FINDER_H
#define STORM_FINDER_H

#include <StormPort.h>
#include <lua.h>

struct Storm_Finder
{
	HANDLE handle;
};

extern void
storm_finder_metatable (lua_State *L);

extern struct Storm_Finder
*storm_finder_access (lua_State *L, int index);

extern struct Storm_Finder
*storm_finder_initialize (lua_State *L);

#endif
