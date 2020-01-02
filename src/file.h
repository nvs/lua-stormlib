#ifndef STORM_FILE_H
#define STORM_FILE_H

#include <StormPort.h>
#include <lua.h>

struct Storm_File
{
	HANDLE handle;
	int is_writable;
};

extern struct Storm_File
*storm_file_access (lua_State *L, int index);

extern struct Storm_File
*storm_file_initialize (lua_State *L);

#endif
