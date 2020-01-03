#ifndef STORM_FINDER_H
#define STORM_FINDER_H

#include <StormPort.h>
#include <lua.h>

struct Storm_MPQ;

#define STORM_FINDER_METATABLE "Storm Finder"

struct Storm_Finder
{
	HANDLE handle;
	struct Storm_MPQ *mpq;
};

extern struct Storm_Finder
*storm_finder_initialize (lua_State *L);

extern struct Storm_Finder
*storm_finder_access (lua_State *L, int index);

#endif
