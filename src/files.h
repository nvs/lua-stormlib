#ifndef STORM_FILES_H
#define STORM_FILES_H

#include "file.h"
#include "mpq.h"
#include <lua.h>

extern void
storm_files_initialize (lua_State *L, const struct Storm_MPQ *mpq);

extern void
storm_files_insert (lua_State *L, const struct Storm_MPQ *mpq, int index);

extern void
storm_files_close (lua_State *L, const struct Storm_MPQ *mpq);

#endif
