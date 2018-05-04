#ifndef STORM_MPQ_H
#define STORM_MPQ_H

#include <StormPort.h>
#include <lua.h>

struct Storm_MPQ
{
	HANDLE handle;
};

extern struct Storm_MPQ
*storm_mpq_initialize (lua_State *L);

extern struct Storm_MPQ
*storm_mpq_access (lua_State *L, int index);

#endif
