#ifndef STORM_MPQ_H
#define STORM_MPQ_H

#include <lua.h>
#include <StormPort.h>

struct Storm_MPQ
{
	HANDLE handle;
};

enum modes {
	READ,
	UPDATE,
	WRITE
};

extern int
storm_mpq_initialize (
	lua_State *L,
	const char *path,
	const enum modes mode);

extern struct Storm_MPQ *
storm_mpq_access (
	lua_State *L,
	int index);

#endif
