#ifndef STORM_HANDLES_H
#define STORM_HANDLES_H

#include "file.h"
#include "finder.h"
#include "mpq.h"
#include <lua.h>

extern void
storm_handles_initialize (lua_State *L, const struct Storm_MPQ *mpq);

extern void
storm_handles_add_file (
	lua_State *L, const struct Storm_MPQ *mpq, int index);

extern void
storm_handles_add_finder (
	lua_State *L, const struct Storm_MPQ *mpq, int index);

extern void
storm_handles_close (lua_State *L, const struct Storm_MPQ *mpq);

#endif
