#ifndef STORM_FILE_H
#define STORM_FILE_H

#include <StormPort.h>
#include <lua.h>

struct Storm_MPQ;

#define STORM_FILE_METATABLE "Storm File"

struct Storm_File
{
	HANDLE handle;
	struct Storm_MPQ *mpq;
	int is_writable;
};

extern struct Storm_File
*storm_file_access (lua_State *L, int index);

extern struct Storm_File
*storm_file_initialize (lua_State *L);

#endif
