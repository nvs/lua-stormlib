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
	char *name;
	int is_writable;
};

extern int
storm_file_initialize (
	lua_State *L,
	struct Storm_MPQ *mpq,
	const char *name,
	const lua_Integer size);

extern struct Storm_File *
storm_file_access (
	lua_State *L,
	int index);

#endif
