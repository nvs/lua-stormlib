#ifndef STORM_HANDLES_H
#define STORM_HANDLES_H

#include <StormPort.h>
#include <lua.h>

struct Storm_File;
struct Storm_Finder;
struct Storm_MPQ;

typedef void
(*Storm_Handles_Callback) (
	lua_State *L,
	void *userdata);

extern void
storm_handles_initialize (
	lua_State *L,
	const struct Storm_MPQ *mpq);

extern void
storm_handles_close (
	lua_State *L,
	const struct Storm_MPQ *mpq);

extern void
storm_handles_iterate_files (
	lua_State* L,
	const struct Storm_MPQ *mpq,
	const Storm_Handles_Callback callback);

extern void
storm_handles_iterate_finders (
	lua_State* L,
	const struct Storm_MPQ *mpq,
	const Storm_Handles_Callback callback);

extern void
storm_handles_add_file (
	lua_State *L,
	int index);

extern void
storm_handles_add_finder (
	lua_State *L,
	int index);

extern void
storm_handles_remove_file (
	lua_State* L,
	const struct Storm_File* file);

extern void
storm_handles_remove_finder (
	lua_State* L,
	const struct Storm_Finder* finder);

#endif
