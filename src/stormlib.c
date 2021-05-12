#include <StormLib.h>
#include <StormPort.h>
#include <compat-5.3.h>
#include <lauxlib.h>
#include <lua.h>
#include <luaconf.h>

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*
 * This module attempts to mirror the StormLib API, within reason.  As such,
 * consistency with StormLib is prioritized over ease of use within Lua.
 * For the most part, the behavior of the wrapped functions should be
 * consistent.
 *
 * Note that there are a few differences.  Specifically, handling
 * `NULL` in various situations (e.g. certain strings and function
 * callbacks).  In these cases, passing `nil` or omitting the argument will
 * work the same as passing `NULL`.  These cases are indicated in the
 * function comments below, and follow the style used within the Lua
 * Reference Manual.
 */
#define STORMLIB_OBJECT_METATABLE "StormLib Handle"

struct object
{
	HANDLE handle;
	SFILECLOSEARCHIVE close;
	HANDLE archive;
	lua_State *compact;
	lua_State *insert;
};

static int
to_error (
	lua_State *L)
{
	const int error = (int) GetLastError ();
	lua_pushnil (L);
	lua_pushstring (L, strerror (error));
	lua_pushinteger (L, error);
	return 3;
}

static int
to_result (
	lua_State *L,
	const bool status)
{
	if (status)
	{
		lua_pushboolean (L, status);
		return 1;
	}

	return to_error (L);
}

typedef bool
(*is_type_function) (
	const struct object *object);

static bool
is_closed (
	const struct object *object)
{
	return object->handle == NULL;
}

static bool
is_archive (
	const struct object *object)
{
	return object->close == SFileCloseArchive;
}

static bool
is_reader (
	const struct object *object)
{
	return object->close == SFileCloseFile;
}

static bool
is_writer (
	const struct object *object)
{
	return object->close == SFileFinishFile;
}

static bool
is_file (
	const struct object *object)
{
	return is_reader (object) || is_writer (object);
}

static bool
is_file_finder (
	const struct object *object)
{
	return object->close == SFileFindClose;
}

static bool
is_listfile_finder (
	const struct object *object)
{
	return object->close == SListFileFindClose;
}

static struct object *
to_object (
	lua_State *L,
	const int index)
{
	return luaL_checkudata (L, index, STORMLIB_OBJECT_METATABLE);
}

/*
 * Do some checks.  StormLib attempts to do these validations on various
 * functions; however, segmentation faults have been observed.  So we err on
 * the side of caution and do it ourselves, until time can be found to look
 * into the issue further.
 *
 * On the flip side, this mimics the behavior of the Lua I/O library, which
 * errors on a closed file.
 */
static HANDLE
to_handle (
	lua_State *L,
	is_type_function is_type)
{
	const struct object *object = to_object (L, 1);

	if (is_closed (object))
	{
		luaL_error (L, "attempt to use a closed handle");
	}

	if (!is_type (object))
	{
		luaL_error (L, "attempt to use an invalid handle");
	}

	return object->handle;
}

static HANDLE
to_archive (
	lua_State *L)
{
	return to_handle (L, is_archive);
}

static HANDLE
to_reader (
	lua_State *L)
{
	return to_handle (L, is_reader);
}

static HANDLE
to_writer (
	lua_State *L)
{
	return to_handle (L, is_writer);
}

static HANDLE
to_file (
	lua_State *L)
{
	return to_handle (L, is_file);
}

static HANDLE
to_file_finder (
	lua_State *L)
{
	return to_handle (L, is_file_finder);
}

static HANDLE
to_listfile_finder (
	lua_State *L)
{
	return to_handle (L, is_listfile_finder);
}

static bool
object_finalize (
	lua_State *L,
	struct object *object)
{
	if (is_archive (object))
	{
		lua_pushnil (L);
		lua_rawsetp (L, LUA_REGISTRYINDEX, &object->compact);
		lua_pushnil (L);
		lua_rawsetp (L, LUA_REGISTRYINDEX, &object->insert);

		lua_rawgetp (L, LUA_REGISTRYINDEX, object->handle);
		lua_pushnil (L);

		while (lua_next (L, -2))
		{
			object_finalize (L, to_object (L, -1));
			lua_pop (L, 1);
		}

		lua_pop (L, 1);
		lua_pushnil (L);
		lua_rawsetp (L, LUA_REGISTRYINDEX, object->handle);
	}
	else
	{
		lua_rawgetp (L, LUA_REGISTRYINDEX, object->archive);
		lua_pushnil (L);
		lua_rawsetp (L, -2, object->handle);
		lua_pop (L, 1);
	}

	const bool status = (*object->close) (object->handle);
	object->handle = NULL;
	return status;
}

static int
object_close (
	lua_State *L)
{
	struct object *object = to_object (L, 1);

	return to_result (
		L, object_finalize (L, object));
}

static int
object_garbage_collect (
	lua_State *L)
{
	struct object *object = to_object (L, 1);

	if (!is_closed (object))
	{
		object_close (L);
	}

	return 0;
}

static int
object_to_string (
	lua_State *L)
{
	const struct object *object = to_object (L, 1);
	const char *text = !is_closed (object) ? "%s (%p)" : "%s (Closed)";
	lua_pushfstring (L, text, STORMLIB_OBJECT_METATABLE, object);
	return 1;
}

static const luaL_Reg
object_methods [] =
{
	{ "__gc", object_garbage_collect },
	{ "__tostring", object_to_string },
	{ NULL, NULL }
};

static int
object_initialize (
	lua_State *L,
	HANDLE handle,
	SFILECLOSEARCHIVE close,
	HANDLE archive)
{
	struct object *object = lua_newuserdata (L, sizeof (*object));
	object->handle = handle;
	object->close = close;
	object->archive = archive;
	object->compact = NULL;
	object->insert = NULL;

	if (luaL_newmetatable (L, STORMLIB_OBJECT_METATABLE))
	{
		luaL_setfuncs (L, object_methods, 0);
	}

	lua_setmetatable (L, -2);

	if (is_archive (object))
	{
		lua_newtable (L);
		lua_rawsetp (L, LUA_REGISTRYINDEX, handle);
	}
	else
	{
		lua_rawgetp (L, LUA_REGISTRYINDEX, archive);
		lua_pushvalue (L, -2);
		lua_rawsetp (L, -2, handle);
		lua_pop (L, 1);
	}

	return 1;
}

/**
 * `SFileSetLocale (locale)`
 */
static int
stormlib_set_locale (
	lua_State *L)
{
	const LCID locale = luaL_checkinteger (L, 1);
	SFileSetLocale (locale);
	return 1;
}

/**
 * `SFileGetLocale ()`
 */
static int
stormlib_get_locale (
	lua_State *L)
{
	const LCID locale = SFileGetLocale ();
	lua_pushinteger (L, locale);
	return 1;
}

/**
 * `SFileOpenArchive (path, flags)`
 */
static int
archive_open (
	lua_State *L)
{
	const char *path = luaL_checkstring (L, 1);
	const DWORD flags = luaL_checkinteger (L, 2);
	HANDLE archive = NULL;

	if (!SFileOpenArchive (path, 0, flags, &archive))
	{
		return to_error (L);
	}

	return object_initialize (L, archive, SFileCloseArchive, NULL);
}

/**
 * `SFileCreateArchive (path, flags, count)`
 */
static int
archive_new (
	lua_State *L)
{
	const char *path = luaL_checkstring (L, 1);
	const DWORD flags = luaL_checkinteger (L, 2);
	const DWORD count = luaL_checkinteger (L, 3);
	HANDLE archive = NULL;

	if (!SFileCreateArchive (path, flags, count, &archive))
	{
		return to_error (L);
	}

	return object_initialize (L, archive, SFileCloseArchive, NULL);
}

/**
 * `SFileCreateArchive2 (path, info)`
 */
static int
archive_new2 (
	lua_State *L)
{
	const char *path = luaL_checkstring (L, 1);
	luaL_checktype (L, 2, LUA_TTABLE);
	lua_settop (L, 2);

	SFILE_CREATE_MPQ info = { 0 };
	info.cbSize = sizeof (info);
	lua_getfield (L, 2, "dwMpqVersion");
	info.dwMpqVersion = luaL_checkinteger (L, -1);
	lua_getfield (L, 2, "dwStreamFlags");
	info.dwStreamFlags = luaL_checkinteger (L, -1);
	lua_getfield (L, 2, "dwFileFlags1");
	info.dwFileFlags1 = luaL_checkinteger (L, -1);
	lua_getfield (L, 2, "dwFileFlags2");
	info.dwFileFlags2 = luaL_checkinteger (L, -1);
	lua_getfield (L, 2, "dwFileFlags3");
	info.dwFileFlags3 = luaL_checkinteger (L, -1);
	lua_getfield (L, 2, "dwAttrFlags");
	info.dwAttrFlags = luaL_checkinteger (L, -1);
	lua_getfield (L, 2, "dwSectorSize");
	info.dwSectorSize = luaL_checkinteger (L, -1);
	lua_getfield (L, 2, "dwRawChunkSize");
	info.dwRawChunkSize = luaL_checkinteger (L, -1);
	lua_getfield (L, 2, "dwMaxFileCount");
	info.dwMaxFileCount = luaL_checkinteger (L, -1);

	HANDLE archive = NULL;

	if (!SFileCreateArchive2 (path, &info, &archive))
	{
		return to_error (L);
	}

	return object_initialize (L, archive, SFileCloseArchive, NULL);
}

/**
 * `SFileFlushArchive (archive)`
 */
static int
archive_flush (
	lua_State *L)
{
	HANDLE archive = to_archive (L);

	return to_result (
		L, SFileFlushArchive (archive));
}

/**
 * `SFileCloseArchive (archive)`
 */
static int
archive_close (
	lua_State *L)
{
	to_archive (L);
	return object_close (L);
}

/**
 * `SFileAddListFile (archive, listfile)`
 */
static int
archive_listfile (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *listfile = luaL_checkstring (L, 2);

	const int status = SFileAddListFile (archive, listfile);
	SetLastError (status);
	return to_result (L, status == ERROR_SUCCESS);
}

static void
compact_callback (
	void *data,
	DWORD work,
	ULONGLONG processed,
	ULONGLONG total)
{
	const struct object *object = data;
	lua_State *L = object->compact;

	lua_rawgetp (L, LUA_REGISTRYINDEX, &object->compact);
	lua_pushinteger (L, work);
	lua_pushinteger (L, (lua_Integer) processed);
	lua_pushinteger (L, (lua_Integer) total);
	lua_call (L, 3, 0);
}

/**
 * `SFileSetCompactCallback (archive [, callback])`
 */
static int
archive_compact_callback (
	lua_State *L)
{
	struct object *object = to_object (L, 1);
	HANDLE archive = to_archive (L);
	SFILE_COMPACT_CALLBACK callback = NULL;

	if (lua_isfunction (L, 2))
	{
		callback = compact_callback;
		object->compact = L;
	}
	else if (!lua_isnoneornil (L, 2))
	{
		luaL_checktype (L, 2, LUA_TFUNCTION);
	}

	lua_settop (L, 2);
	lua_rawsetp (L, LUA_REGISTRYINDEX, &object->compact);

	return to_result (
		L, SFileSetCompactCallback (archive, callback, object));
}

/**
 * `SFileCompactArchive (archive [, listfile])`
 */
static int
archive_compact (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *listfile = luaL_optstring (L, 2, NULL);

	return to_result (
		L, SFileCompactArchive (archive, listfile, 0));
}

/**
 * `SFileGetMaxFileCount (archive)`
 */
static int
archive_get_limit (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	DWORD count = SFileGetMaxFileCount (archive);
	lua_pushinteger (L, count);
	return 1;
}

/**
 * `SFileSetMaxFileCount (archive, limit)`
 */
static int
archive_set_limit (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const DWORD limit = luaL_checkinteger (L, 2);

	return to_result (
		L, SFileSetMaxFileCount (archive, limit));
}

/*
 * `SFileOpenPatchArchive (archive, path [, prefix])`
 */
static int
archive_patch (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *path = luaL_checkstring (L, 2);
	const char *prefix = luaL_optstring (L, 3, NULL);

	return to_result (
		L, SFileOpenPatchArchive (archive, path, prefix, 0));
}

/*
 * `SFileIsPatchedArchive (archive)`
 */
static int
archive_is_patched (
	lua_State *L)
{
	HANDLE archive = to_archive (L);

	return to_result (
		L, SFileIsPatchedArchive (archive));
}

/*
 * `SFileHasFile (archive, name)`
 */
static int
archive_has (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *name = luaL_checkstring (L, 2);

	return to_result (
		L, SFileHasFile (archive, name));
}

/**
 * `SFileOpenFileEx (archive, name, scope)`
 */
static int
reader_open (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *name = luaL_checkstring (L, 2);
	const DWORD scope = luaL_checkinteger (L, 3);
	HANDLE reader = NULL;

	if (!SFileOpenFileEx (archive, name, scope, &reader))
	{
		return to_error (L);
	}

	return object_initialize (L, reader, SFileCloseFile, archive);
}

/*
 * `SFileGetFileSize (reader)
 */
static int
file_size (
	lua_State *L)
{
	HANDLE file = to_file (L);
	DWORD size = SFileGetFileSize (file, NULL);

	if (size == SFILE_INVALID_SIZE)
	{
		return to_error (L);
	}

	lua_pushinteger (L, size);
	return 1;
}

/**
 * `SFileSetFilePointer (file, offset, mode)`
 */
static int
file_seek (
	lua_State *L)
{
	HANDLE file = to_file (L);
	const LONGLONG offset = luaL_checkinteger (L, 2);
	const DWORD mode = luaL_checkinteger (L, 3);

	LONG high = (LONG) (offset >> 32);
	DWORD low = SFileSetFilePointer (file, (LONG) offset, &high, mode);

	if (low == SFILE_INVALID_POS)
	{
		return to_error (L);
	}

	lua_pushinteger (L, (lua_Integer) high | low);
	return 1;
}

/**
 * `SFileReadFile (file, bytes_to_read)`
 */
static int
file_read (
	lua_State *L)
{
	HANDLE file = to_file (L);
	const DWORD bytes_to_read = luaL_checkinteger (L, 2);

	luaL_Buffer buffer;
	char *bytes = luaL_buffinitsize (L, &buffer, bytes_to_read);
	DWORD bytes_read = 0;

	if (!SFileReadFile (file, bytes, bytes_to_read, &bytes_read, NULL)
		&& GetLastError () != ERROR_HANDLE_EOF)
	{
		return to_error (L);
	}

	luaL_pushresultsize (&buffer, bytes_read);
	return 1;
}

/**
 * `SFileCloseFile (file)`
 */
static int
reader_close (
	lua_State *L)
{
	to_reader (L);
	return object_close (L);
}

typedef int
(*info_function) (
	lua_State *L,
	void *buffer,
	const DWORD size);

static int
info_not_implemented_yet (
	lua_State *L)
{
	lua_pushnil (L);
	lua_pushliteral (L, "info class not implemented yet");
	return 2;
}

static int
info_integer32 (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	DWORD *info = buffer;
	lua_pushinteger (L, (lua_Integer) *info);
	return 1;
}

static int
info_integer64 (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	ULONGLONG *info = buffer;
	lua_pushinteger (L, (lua_Integer) *info);
	return 1;
}

static int
info_string (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	lua_pushstring (L, buffer);
	return 1;
}

static int
info_string_array (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	lua_newtable (L);
	int count = 1;

	for (const char *name = buffer;
		name [0] != '\0';
		name = name + strlen (name) + 1)
	{
		lua_pushstring (L, name);
		lua_rawseti (L, -2, count++);
	}

	return 1;
}

static int
info_mpq_header (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	return info_not_implemented_yet (L);
}

static int
info_het_header (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	return info_not_implemented_yet (L);
}

static int
info_het_table (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	return info_not_implemented_yet (L);
}

static int
info_bet_header (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	return info_not_implemented_yet (L);
}

static int
info_bet_table (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	return info_not_implemented_yet (L);
}

static int
info_file_entry (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	TFileEntry *info = buffer;

	lua_createtable (L, 0, 8);
	lua_pushinteger (L, (lua_Integer) info->FileNameHash);
	lua_setfield (L, -2, "FileNameHash");
	lua_pushinteger (L, (lua_Integer) info->ByteOffset);
	lua_setfield (L, -2, "ByteOffset");
	lua_pushinteger (L, (lua_Integer) info->FileTime);
	lua_setfield (L, -2, "FileTime");
	lua_pushinteger (L, info->dwFileSize);
	lua_setfield (L, -2, "dwFileSize");
	lua_pushinteger (L, info->dwCmpSize);
	lua_setfield (L, -2, "dwCmpSize");
	lua_pushinteger (L, info->dwFlags);
	lua_setfield (L, -2, "dwFlags");
	lua_pushinteger (L, info->dwCrc32);
	lua_setfield (L, -2, "dwCrc32");
	lua_pushstring (L, (const char *) info->md5);
	lua_setfield (L, -2, "md5");
	lua_pushstring (L, info->szFileName);
	lua_setfield (L, -2, "szFileName");

	return 1;
}

static void
info_load_hash_entry (
	lua_State *L,
	const TMPQHash *info)
{
	lua_createtable (L, 0, 6);
	lua_pushinteger (L, info->dwName1);
	lua_setfield (L, -2, "dwName1");
	lua_pushinteger (L, info->dwName2);
	lua_setfield (L, -2, "dwName2");
	lua_pushinteger (L, info->lcLocale);
	lua_setfield (L, -2, "lcLocale");
	lua_pushinteger (L, info->Platform);
	lua_setfield (L, -2, "Platform");
	lua_pushinteger (L, info->Reserved);
	lua_setfield (L, -2, "Reserved");
	lua_pushinteger (L, info->dwBlockIndex);
	lua_setfield (L, -2, "dwBlockIndex");
}

static int
info_hash_entry (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	info_load_hash_entry (L, buffer);
	return 1;
}

static int
info_hash_table (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	const TMPQHash *info = buffer;
	const size_t count = size / sizeof (TMPQHash);
	lua_createtable (L, (int) count, 0);

	for (int i = 0; i < count; i++, info++)
	{
		info_load_hash_entry (L, info);
		lua_rawseti (L, -2, i + 1);
	}

	return 1;
}

static int
info_block_table (
	lua_State *L,
	void *buffer,
	const DWORD size)
{
	const TMPQBlock *info = buffer;
	const size_t count = size / sizeof (TMPQBlock);
	lua_createtable (L, (int) count, 0);

	for (int i = 0; i < count; i++, info++)
	{
		lua_createtable (L, 0, 4);
		lua_pushinteger (L, info->dwFilePos);
		lua_setfield (L, -2, "dwFilePos");
		lua_pushinteger (L, info->dwCSize);
		lua_setfield (L, -2, "dwCsize");
		lua_pushinteger (L, info->dwFSize);
		lua_setfield (L, -2, "dwFSize");
		lua_pushinteger (L, info->dwFlags);
		lua_setfield (L, -2, "dwFlags");
		lua_rawseti (L, -2, i + 1);
	}

	return 1;
}

static int
info_helper (
	lua_State *L,
	HANDLE (to_handle) (
		lua_State *L),
	const SFileInfoClass class,
	info_function info)
{
	HANDLE handle = to_handle (L);
	DWORD size = 0;
	SFileGetFileInfo (handle, class, NULL, 0, &size);

	/*
	 * Passing a `NULL` buffer here will elicit a related error.  It will
	 * also cause a valid size to be set.  We leverage that fact.
	 */
	if (!SFileGetFileInfo (handle, class, NULL, 0, &size)
		&& GetLastError () != ERROR_INSUFFICIENT_BUFFER)
	{
		return to_error (L);
	}

	/*
	 * Unsure if StormLib will return zero size.  But if it does, we will
	 * consider it an error and return accordingly.
	 */
	if (size == 0)
	{
		SetLastError (ERROR_NOT_ENOUGH_MEMORY);
		return to_error (L);
	}

	void *buffer = calloc (1, size);
	if (buffer == NULL)
	{
		SetLastError (ERROR_NOT_ENOUGH_MEMORY);
		return to_error (L);
	}

	int result = 0;
	if (SFileGetFileInfo (handle, class, buffer, size, NULL))
	{
		result = info (L, buffer, size);
	}
	else
	{
		result = to_error (L);
	}

	free (buffer);
	return result;
}

static int
info_archive (
	lua_State *L,
	const SFileInfoClass class,
	info_function info)
{
	return info_helper (L, to_archive, class, info);
}

static int
info_file (
	lua_State *L,
	const SFileInfoClass class,
	info_function info)
{
	return info_helper (L, to_file, class, info);
}

/**
 * `SFileGetFileInfo (file, class)`
 */
static int
stormlib_info (
	lua_State *L)
{
	const SFileInfoClass class = luaL_checkinteger (L, 2);

	switch (class)
	{
		case SFileMpqFileName:
		case SFileMpqStreamBitmap:
		{
			return info_archive (L, class, info_string);
		}

		case SFileMpqUserDataOffset:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqUserDataHeader:
		case SFileMpqUserData:
		{
			HANDLE handle = to_archive (L);

			/*
			 * Ensure that the MPQ user data exists, else we may run into a
			 * segmentation fault.
			 *
			 * TODO: StormLib 9.24: Remove this check.
			 */
			if (((TMPQArchive *) handle)->pUserData == NULL)
			{
				SetLastError (ERROR_INVALID_PARAMETER);
				return to_error (L);
			}

			return info_archive (L, class, info_string);
		}

		case SFileMpqHeaderOffset:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqHeaderSize:
		{
			return info_archive (L, class, info_integer32);
		}

		case SFileMpqHeader:
		{
			return info_archive (L, class, info_mpq_header);
		}

		case SFileMpqHetTableOffset:
		case SFileMpqHetTableSize:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqHetHeader:
		{
			return info_archive (L, class, info_het_header);
		}

		case SFileMpqHetTable:
		{
			return info_archive (L, class, info_het_table);
		}

		case SFileMpqBetTableOffset:
		case SFileMpqBetTableSize:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqBetHeader:
		{
			return info_archive (L, class, info_bet_header);
		}

		case SFileMpqBetTable:
		{
			return info_archive (L, class, info_bet_table);
		}

		case SFileMpqHashTableOffset:
		case SFileMpqHashTableSize64:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqHashTableSize:
		{
			return info_archive (L, class, info_integer32);
		}

		case SFileMpqHashTable:
		{
			return info_archive (L, class, info_hash_table);
		}

		case SFileMpqBlockTableOffset:
		case SFileMpqBlockTableSize64:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqBlockTableSize:
		{
			return info_archive (L, class, info_integer32);
		}

		case SFileMpqBlockTable:
		{
			return info_archive (L, class, info_block_table);
		}

		case SFileMpqHiBlockTableOffset:
		case SFileMpqHiBlockTableSize64:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqHiBlockTable:
		{
			return info_not_implemented_yet (L);
		}

		case SFileMpqSignatures:
		{
			return info_archive (L, class, info_integer32);
		}

		case SFileMpqStrongSignatureOffset:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqStrongSignatureSize:
		{
			return info_archive (L, class, info_integer32);
		}

		case SFileMpqStrongSignature:
		{
			return info_archive (L, class, info_string);
		}

		case SFileMpqArchiveSize64:
		{
			return info_archive (L, class, info_integer64);
		}

		case SFileMpqArchiveSize:
		case SFileMpqMaxFileCount:
		case SFileMpqFileTableSize:
		case SFileMpqSectorSize:
		case SFileMpqNumberOfFiles:
		case SFileMpqRawChunkSize:
		case SFileMpqStreamFlags:
		case SFileMpqFlags:
		{
			return info_archive (L, class, info_integer32);
		}

		case SFileInfoPatchChain:
		{
			return info_file (L, class, info_string_array);
		}

		case SFileInfoFileEntry:
		{
			return info_file (L, class, info_file_entry);
		}

		case SFileInfoHashEntry:
		{
			return info_file (L, class, info_hash_entry);
		}

		case SFileInfoHashIndex:
		case SFileInfoNameHash1:
		case SFileInfoNameHash2:
		{
			return info_file (L, class, info_integer32);
		}

		case SFileInfoNameHash3:
		{
			return info_file (L, class, info_integer64);
		}

		case SFileInfoLocale:
		case SFileInfoFileIndex:
		{
			return info_file (L, class, info_integer32);
		}

		case SFileInfoByteOffset:
		case SFileInfoFileTime:
		{
			return info_file (L, class, info_integer64);
		}

		case SFileInfoFileSize:
		case SFileInfoCompressedSize:
		case SFileInfoFlags:
		case SFileInfoEncryptionKey:
		case SFileInfoEncryptionKeyRaw:
		case SFileInfoCRC32:
		{
			return info_file (L, class, info_integer32);
		}

		default:
		{
			return luaL_argerror (L, 2, "invalid info class");
		}
	}
}

/**
 * `SFileGetFileName (file)`
 */
static int
file_name (
	lua_State *L)
{
	HANDLE file = to_file (L);
	char name [MAX_PATH + 1] = { 0 };

	if (!SFileGetFileName (file, name))
	{
		return to_error (L);
	}

	lua_pushstring (L, name);
	return 1;
}

/*
 * `SFileExtractFile (archive, name, path)`
 */
static int
archive_extract (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *name = luaL_checkstring (L, 2);
	const char *path = luaL_checkstring (L, 3);
	const DWORD scope = luaL_checkinteger (L, 4);

	return to_result (
		L, SFileExtractFile (archive, name, path, scope));
}

/*
 * `SFileVerifyFile (archive)`
 */
static int
archive_verify (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *name = luaL_checkstring (L, 2);
	const DWORD flags = luaL_checkinteger (L, 3);
	const DWORD result = SFileVerifyFile (archive, name, flags);

	if (result & VERIFY_OPEN_ERROR)
	{
		return to_error (L);
	}

	lua_pushinteger (L, result);
	return 1;
}

/**
 * `SFileSignArchive (archive, type)`
 */
static int
archive_sign (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const DWORD type = luaL_checkinteger (L, 2);

	return to_result (
		L, SFileSignArchive (archive, type));
}

/*
 * `SFileVerifyArchive (archive)`
 */
static int
stormlib_verify (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const DWORD result = SFileVerifyArchive (archive);

	lua_pushinteger (L, result);
	return 1;
}

static void
finder_load_data (
	lua_State *L,
	SFILE_FIND_DATA data)
{
	lua_createtable (L, 0, 10);
	lua_pushstring (L, data.cFileName);
	lua_setfield (L, -2, "cFileName");
	lua_pushstring (L, data.szPlainName);
	lua_setfield (L, -2, "szPlainName");
	lua_pushinteger (L, data.dwHashIndex);
	lua_setfield (L, -2, "dwHashIndex");
	lua_pushinteger (L, data.dwBlockIndex);
	lua_setfield (L, -2, "dwBlockIndex");
	lua_pushinteger (L, data.dwFileSize);
	lua_setfield (L, -2, "dwFileSize");
	lua_pushinteger (L, data.dwFileFlags);
	lua_setfield (L, -2, "dwFileFlags");
	lua_pushinteger (L, data.dwCompSize);
	lua_setfield (L, -2, "dwCompSize");
	lua_pushinteger (L, data.dwFileTimeLo);
	lua_setfield (L, -2, "dwFileTimeLo");
	lua_pushinteger (L, data.dwFileTimeHi);
	lua_setfield (L, -2, "dwFileTimeHi");
	lua_pushinteger (L, data.lcLocale);
	lua_setfield (L, -2, "lcLocale");
}

/**
 * `SFileFindFirstFile (archive, mask [, listfile])`
 */
static int
file_finder_open (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *mask = luaL_checkstring (L, 2);
	const char *listfile = luaL_optstring (L, 3, NULL);

	SFILE_FIND_DATA data;
	HANDLE finder = SFileFindFirstFile (archive, mask, &data, listfile);

	if (finder == NULL)
	{
		return to_error (L);
	}

	object_initialize (L, finder, SFileFindClose, archive);
	finder_load_data (L, data);
	return 2;
}

/**
 * `SFileFindNextFile (finder)`
 */
static int
file_finder_next (
	lua_State *L)
{
	HANDLE finder = to_file_finder (L);
	SFILE_FIND_DATA data;

	if (!SFileFindNextFile (finder, &data))
	{
		return to_error (L);
	}

	finder_load_data (L, data);
	return 1;
}

/**
 * `SFileFindClose (finder)`
 */
static int
file_finder_close (
	lua_State *L)
{
	to_file_finder (L);
	return object_close (L);
}

/*
 * `SListFileFindFirstFile (archive, mask [, listfile])`
 */
static int
listfile_finder_open (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *mask = luaL_checkstring (L, 2);
	const char *listfile = luaL_optstring (L, 3, NULL);

	SFILE_FIND_DATA data;
	HANDLE finder = SListFileFindFirstFile (archive, listfile, mask, &data);

	if (finder == NULL)
	{
		return to_error (L);
	}

	object_initialize (L, finder, SListFileFindClose, archive);
	lua_pushstring (L, data.cFileName);
	return 2;
}

/**
 * `SListFileFineNextFile (finder)`
 */
static int
listfile_finder_next (
	lua_State *L)
{
	HANDLE finder = to_listfile_finder (L);
	SFILE_FIND_DATA data;

	if (!SListFileFindNextFile (finder, &data))
	{
		return to_error (L);
	}

	lua_pushstring (L, data.cFileName);
	return 1;
}

/**
 * `SListFileFindClose (finder)`
 */
static int
listfile_finder_close (
	lua_State *L)
{
	to_listfile_finder (L);
	return object_close (L);
}

/*
 * A basic wrapper around `SFileEnumLocales` to make it behave closer to
 * most of the functions in the public API.  That is, returning `true` on
 * success, and returning `false` on failure with an error accessible via
 * `GetLastError`.
 */
static bool
SFileEnumLocalesWrapper (
	HANDLE archive,
	const char *name,
	LCID *locales,
	DWORD *count)
{
	const int status = SFileEnumLocales (archive, name, locales, count, 0);
	SetLastError (status);
	return status == ERROR_SUCCESS;
}

/**
 * `SFileEnumLocales (archive, name)`
 */
static int
archive_locales (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *name = luaL_checkstring (L, 2);

	/*
	 * Passing a `NULL` buffer here will elicit a related error.  Success
	 * can also be returned as well, as is the case with a nonexistent file.
	 * In these cases, a valid count will be set.
	 */
	DWORD count = 0;
	if (!SFileEnumLocalesWrapper (archive, name, NULL, &count)
		&& GetLastError () != ERROR_INSUFFICIENT_BUFFER)
	{
		return to_error (L);
	}

	lua_createtable (L, (int) count, 0);
	if (count == 0)
	{
		return 1;
	}

	LCID *locales = calloc (count, sizeof (LCID));
	if (locales == NULL)
	{
		SetLastError (ERROR_NOT_ENOUGH_MEMORY);
		return to_error (L);
	}

	int result = 1;
	if (SFileEnumLocalesWrapper (archive, name, locales, &count))
	{
		for (int i = 0; i < count; i++)
		{
			lua_pushinteger (L, locales [i]);
			lua_rawseti (L, -2, i + 1);
		}
	}
	else
	{
		result = to_error (L);
	}

	free (locales);
	return result;
}

/**
 * `SFileCreateFile (archive, name, time, size, locale, flags)`
 */
static int
writer_open (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *name = luaL_checkstring (L, 2);
	const ULONGLONG time = luaL_checkinteger (L, 3);
	const DWORD size = luaL_checkinteger (L, 4);
	const LCID locale = luaL_checkinteger (L, 5);
	const DWORD flags = luaL_checkinteger (L, 6);
	HANDLE writer = NULL;

	if (!SFileCreateFile (
		archive, name, time, size, locale, flags, &writer))
	{
		if (writer)
		{
			SFileFinishFile (writer);
		}

		return to_error (L);
	}

	return object_initialize (L, writer, SFileFinishFile, archive);
}

/**
 * `SFileWriteFile (file, data, compression)`
 */
static int
writer_write (
	lua_State *L)
{
	HANDLE file = to_writer (L);
	size_t size;
	const char *data = luaL_checklstring (L, 2, &size);
	const DWORD compression = luaL_checkinteger (L, 3);

	return to_result (
		L, SFileWriteFile (file, data, size, compression));
}

/**
 * `SFileFinishFile (file)`
 */
static int
writer_close (
	lua_State *L)
{
	to_writer (L);
	return object_close (L);
}

/**
 * `SFileAddFileEx (
 *     archive, path, name, flags,
 *     compression, compression_next)`
 */
static int
archive_insert (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *path = luaL_checkstring (L, 2);
	const char *name = luaL_checkstring (L, 3);
	const DWORD flags = luaL_checkinteger (L, 4);
	const DWORD compression = luaL_checkinteger (L, 5);
	const DWORD compression_next = luaL_checkinteger (L, 6);

	return to_result (
		L, SFileAddFileEx (
			archive, path, name, flags,
			compression, compression_next));
}

/**
 * `SFileRemoveFile (archive, name)`
 */
static int
archive_remove (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *name = luaL_checkstring (L, 2);

	return to_result (
		L, SFileRemoveFile (archive, name, 0));
}

/**
 * `SFileRenameFile (archive, old, new)`
 */
static int
archive_rename (
	lua_State *L)
{
	HANDLE archive = to_archive (L);
	const char *old = luaL_checkstring (L, 2);
	const char *new = luaL_checkstring (L, 3);

	return to_result (
		L, SFileRenameFile (archive, old, new));
}

/**
 * `SFileSetFileLocale (file, locale)`
 */
static int
file_locale (
	lua_State *L)
{
	HANDLE file = to_file (L);
	const LCID locale = luaL_checkinteger (L, 2);

	return to_result (
		L, SFileSetFileLocale (file, locale));
}

static void
insert_callback (
	void *data,
	DWORD written,
	/* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) */
	DWORD total,
	char finished)
{
	const struct object *object = data;
	lua_State *L = object->insert;

	lua_rawgetp (L, LUA_REGISTRYINDEX, &object->insert);
	lua_pushinteger (L, written);
	lua_pushinteger (L, total);
	lua_pushboolean (L, finished);
	lua_call (L, 3, 0);
}

/**
 * `SFileSetAddFileCallback (archive [, callback])`
 */
static int
archive_insert_callback (
	lua_State *L)
{
	struct object *object = to_object (L, 1);
	HANDLE archive = to_archive (L);
	SFILE_ADDFILE_CALLBACK callback = NULL;

	if (lua_isfunction (L, 2))
	{
		callback = insert_callback;
		object->insert = L;
	}
	else if (!lua_isnoneornil (L, 2))
	{
		luaL_checktype (L, 2, LUA_TFUNCTION);
	}

	lua_settop (L, 2);
	lua_rawsetp (L, LUA_REGISTRYINDEX, &object->insert);

	return to_result (
		L, SFileSetAddFileCallback (archive, callback, object));
}

/**
 * `SCompCompress (in, compression [, level])`
 */
static int
stormlib_compress (
	lua_State *L)
{
	size_t in_size;
	const char *in = luaL_checklstring (L, 1, &in_size);
	const int compression = (int) luaL_checkinteger (L, 2);
	const int level = (int) luaL_optinteger (L, 3, 0);

	/*
	 * StormLib's compression accepts `int`.  It also 'fails' if the output
	 * would otherwise exceed the input length, and returns the input.
	 */
	if (in_size > INT_MAX)
	{
		const char *message = lua_pushfstring (
			L, "input length exceeded: %d", INT_MAX);
		return luaL_argerror (L, 1, message);
	}

	void *in_copy = malloc (in_size);
	if (in_copy == NULL)
	{
		SetLastError (ERROR_NOT_ENOUGH_MEMORY);
		return to_error (L);
	}

	memcpy (in_copy, in, in_size);
	luaL_Buffer buffer;
	int out_size = (int) in_size;
	char *out = luaL_buffinitsize (L, &buffer, out_size);

	int result = 1;
	if (SCompCompress (
		out, &out_size, in_copy, (int) in_size, compression, 0, level))
	{
		luaL_pushresultsize (&buffer, out_size);
	}
	else
	{
		result = to_error (L);
	}

	free (in_copy);
	return result;
}

/**
 * `SCompDecompress (in, out_size)`
 */
static int
stormlib_decompress (
	lua_State *L)
{
	size_t in_size;
	const char *in = luaL_checklstring (L, 1, &in_size);
	int out_size = (int) luaL_checkinteger (L, 2);

	if (in_size > INT_MAX)
	{
		const char *message = lua_pushfstring (
			L, "input length exceeded: %d", INT_MAX);
		return luaL_argerror (L, 1, message);
	}

	void *in_copy = malloc (in_size);
	if (in_copy == NULL)
	{
		SetLastError (ERROR_NOT_ENOUGH_MEMORY);
		return to_error (L);
	}

	memcpy (in_copy, in, in_size);
	luaL_Buffer buffer;
	char *out = luaL_buffinitsize (L, &buffer, out_size);

	int result = 1;
	if (SCompDecompress (out, &out_size, in_copy, (int) in_size))
	{
		luaL_pushresultsize (&buffer, out_size);
	}
	else
	{
		result = to_error (L);
	}

	free (in_copy);
	return result;
}

/*
 * Ordered, as found in the StormLib.h.
 */
static const luaL_Reg
stormlib_functions [] =
{
	{ "SFileSetLocale", stormlib_set_locale },
	{ "SFileGetLocale", stormlib_get_locale },

	{ "SFileOpenArchive", archive_open },
	{ "SFileCreateArchive", archive_new },
	{ "SFileCreateArchive2", archive_new2 },

	/* SFileSetDownloadCallback: Not implemented. */
	{ "SFileFlushArchive", archive_flush },
	{ "SFileCloseArchive", archive_close },

	{ "SFileAddListFile", archive_listfile },

	{ "SFileSetCompactCallback", archive_compact_callback },
	{ "SFileCompactArchive", archive_compact },

	{ "SFileGetMaxFileCount", archive_get_limit },
	{ "SFileSetMaxFileCount", archive_set_limit },

	/* SFileGetAttributes: Not implemented. */
	/* SFileSetAttributes: Not implemented. */
	/* SFileUpdateFileAttributes: Not implemented. */

	{ "SFileOpenPatchArchive", archive_patch },
	{ "SFileIsPatchedArchive", archive_is_patched },

	{ "SFileHasFile", archive_has },
	{ "SFileOpenFileEx", reader_open },
	{ "SFileGetFileSize", file_size },
	{ "SFileSetFilePointer", file_seek },
	{ "SFileReadFile", file_read },
	{ "SFileCloseFile", reader_close },

	{ "SFileGetFileInfo", stormlib_info },
	{ "SFileGetFileName", file_name },
	/* SFileFreeFileInfo: Not implemented. */

	{ "SFileExtractFile", archive_extract },

	/* SFileGetFileChecksums: Not implemented. */
	{ "SFileVerifyFile", archive_verify },
	/* SFileVerifyRawData: Not implemented. */
	{ "SFileSignArchive", archive_sign },
	{ "SFileVerifyArchive", stormlib_verify },

	{ "SFileFindFirstFile", file_finder_open },
	{ "SFileFindNextFile", file_finder_next },
	{ "SFileFindClose", file_finder_close },

	{ "SListFileFindFirstFile", listfile_finder_open },
	{ "SListFileFindNextFile", listfile_finder_next },
	{ "SListFileFindClose", listfile_finder_close },

	{ "SFileEnumLocales", archive_locales },

	{ "SFileCreateFile", writer_open },
	{ "SFileWriteFile", writer_write },
	{ "SFileFinishFile", writer_close },

	{ "SFileAddFileEx", archive_insert },
	/* SFileAddFile: Obsolete. */
	/* SFileAddWave: Obsolete. */
	{ "SFileRemoveFile", archive_remove },
	{ "SFileRenameFile", archive_rename },
	{ "SFileSetFileLocale", file_locale },
	/* SFileSetDataCompression: Obsolete. */
	{ "SFileSetAddFileCallback", archive_insert_callback },

	/* SCompImplode: Use SCompCompress with MPQ_COMPRESSION_PKWARE. */
	/* SCompExplode: Use SCompDecompress with MPQ_COMPRESSION_PKWARE. */
	{ "SCompCompress", stormlib_compress },
	{ "SCompDecompress", stormlib_decompress },
	/* SCompDecompress2: Not implemented. */

	{ NULL, NULL }
};

#define lua_stormlib_integer(L, name) \
	lua_pushinteger(L, name); \
	lua_setfield(L, -2, #name)

extern int
luaopen_stormlib_core (
	lua_State *L)
{
	luaL_newlib (L, stormlib_functions);

	/*
	 * Error codes from StormPort.h.  Making the assumption that these are
	 * all the error codes used in StormLib.
	 */
	lua_stormlib_integer (L, ERROR_SUCCESS);
	lua_stormlib_integer (L, ERROR_FILE_NOT_FOUND);
	lua_stormlib_integer (L, ERROR_ACCESS_DENIED);
	lua_stormlib_integer (L, ERROR_INVALID_HANDLE);
	lua_stormlib_integer (L, ERROR_NOT_ENOUGH_MEMORY);
	lua_stormlib_integer (L, ERROR_NOT_SUPPORTED);
	lua_stormlib_integer (L, ERROR_INVALID_PARAMETER);
	lua_stormlib_integer (L, ERROR_NEGATIVE_SEEK);
	lua_stormlib_integer (L, ERROR_DISK_FULL);
	lua_stormlib_integer (L, ERROR_ALREADY_EXISTS);
	lua_stormlib_integer (L, ERROR_INSUFFICIENT_BUFFER);
	lua_stormlib_integer (L, ERROR_BAD_FORMAT);
	lua_stormlib_integer (L, ERROR_NO_MORE_FILES);
	lua_stormlib_integer (L, ERROR_HANDLE_EOF);
	lua_stormlib_integer (L, ERROR_CAN_NOT_COMPLETE);
	lua_stormlib_integer (L, ERROR_FILE_CORRUPT);

	/* For `SFileOpenArchive ()`. */
	lua_stormlib_integer (L, BASE_PROVIDER_FILE);
	lua_stormlib_integer (L, BASE_PROVIDER_MAP);
	lua_stormlib_integer (L, BASE_PROVIDER_HTTP);

	lua_stormlib_integer (L, STREAM_PROVIDER_FLAT);
	lua_stormlib_integer (L, STREAM_PROVIDER_PARTIAL);
	lua_stormlib_integer (L, STREAM_PROVIDER_MPQE);
	lua_stormlib_integer (L, STREAM_PROVIDER_BLOCK4);

	lua_stormlib_integer (L, STREAM_FLAG_READ_ONLY);
	lua_stormlib_integer (L, STREAM_FLAG_WRITE_SHARE);
	lua_stormlib_integer (L, STREAM_FLAG_USE_BITMAP);

	lua_stormlib_integer (L, MPQ_OPEN_NO_LISTFILE);
	lua_stormlib_integer (L, MPQ_OPEN_NO_ATTRIBUTES);
	lua_stormlib_integer (L, MPQ_OPEN_NO_HEADER_SEARCH);
	lua_stormlib_integer (L, MPQ_OPEN_FORCE_MPQ_V1);
	lua_stormlib_integer (L, MPQ_OPEN_CHECK_SECTOR_CRC);
	/* MPQ_OPEN_READ_ONLY: Deprecated.  Use STREAM_FLAG_READ_ONLY. */
	/* MPQ_OPEN_ENCRYPTED: Deprecated.  Use STREAM_PROVIDER_MPQE. */
	lua_stormlib_integer (L, MPQ_OPEN_FORCE_LISTFILE);

	/* For `SFileCreateArchive ()`. */
	lua_stormlib_integer (L, MPQ_CREATE_LISTFILE);
	lua_stormlib_integer (L, MPQ_CREATE_ATTRIBUTES);
	lua_stormlib_integer (L, MPQ_CREATE_SIGNATURE);
	lua_stormlib_integer (L, MPQ_CREATE_ARCHIVE_V1);
	lua_stormlib_integer (L, MPQ_CREATE_ARCHIVE_V2);
	lua_stormlib_integer (L, MPQ_CREATE_ARCHIVE_V3);
	lua_stormlib_integer (L, MPQ_CREATE_ARCHIVE_V4);

	lua_stormlib_integer (L, HASH_TABLE_SIZE_MIN);
	lua_stormlib_integer (L, HASH_TABLE_SIZE_DEFAULT);
	lua_stormlib_integer (L, HASH_TABLE_SIZE_MAX);

	/* For `SFileSignArchive ()`. */
	lua_stormlib_integer (L, SIGNATURE_TYPE_NONE);
	lua_stormlib_integer (L, SIGNATURE_TYPE_WEAK);
	lua_stormlib_integer (L, SIGNATURE_TYPE_STRONG);

	/* For `SFileOpenFileEx ()`. */
	lua_stormlib_integer (L, SFILE_OPEN_FROM_MPQ);
	lua_stormlib_integer (L, SFILE_OPEN_CHECK_EXISTS);
	lua_stormlib_integer (L, SFILE_OPEN_LOCAL_FILE);

	/* For `SFileSetFilePointer ()`. */
	lua_stormlib_integer (L, FILE_BEGIN);
	lua_stormlib_integer (L, FILE_CURRENT);
	lua_stormlib_integer (L, FILE_END);

	/* For `SFileGetFileInfo ()`. */
	lua_stormlib_integer (L, SFileMpqFileName);
	lua_stormlib_integer (L, SFileMpqStreamBitmap);
	lua_stormlib_integer (L, SFileMpqUserDataOffset);
	lua_stormlib_integer (L, SFileMpqUserDataHeader);
	lua_stormlib_integer (L, SFileMpqUserData);
	lua_stormlib_integer (L, SFileMpqHeaderOffset);
	lua_stormlib_integer (L, SFileMpqHeaderSize);
	lua_stormlib_integer (L, SFileMpqHeader);
	lua_stormlib_integer (L, SFileMpqHetTableOffset);
	lua_stormlib_integer (L, SFileMpqHetTableSize);
	lua_stormlib_integer (L, SFileMpqHetHeader);
	lua_stormlib_integer (L, SFileMpqHetTable);
	lua_stormlib_integer (L, SFileMpqBetTableOffset);
	lua_stormlib_integer (L, SFileMpqBetTableSize);
	lua_stormlib_integer (L, SFileMpqBetHeader);
	lua_stormlib_integer (L, SFileMpqBetTable);
	lua_stormlib_integer (L, SFileMpqHashTableOffset);
	lua_stormlib_integer (L, SFileMpqHashTableSize64);
	lua_stormlib_integer (L, SFileMpqHashTableSize);
	lua_stormlib_integer (L, SFileMpqHashTable);
	lua_stormlib_integer (L, SFileMpqBlockTableOffset);
	lua_stormlib_integer (L, SFileMpqBlockTableSize64);
	lua_stormlib_integer (L, SFileMpqBlockTableSize);
	lua_stormlib_integer (L, SFileMpqBlockTable);
	lua_stormlib_integer (L, SFileMpqHiBlockTableOffset);
	lua_stormlib_integer (L, SFileMpqHiBlockTableSize64);
	lua_stormlib_integer (L, SFileMpqHiBlockTable);
	lua_stormlib_integer (L, SFileMpqSignatures);
	lua_stormlib_integer (L, SFileMpqStrongSignatureOffset);
	lua_stormlib_integer (L, SFileMpqStrongSignatureSize);
	lua_stormlib_integer (L, SFileMpqStrongSignature);
	lua_stormlib_integer (L, SFileMpqArchiveSize64);
	lua_stormlib_integer (L, SFileMpqArchiveSize);
	lua_stormlib_integer (L, SFileMpqMaxFileCount);
	lua_stormlib_integer (L, SFileMpqFileTableSize);
	lua_stormlib_integer (L, SFileMpqSectorSize);
	lua_stormlib_integer (L, SFileMpqNumberOfFiles);
	lua_stormlib_integer (L, SFileMpqRawChunkSize);
	lua_stormlib_integer (L, SFileMpqStreamFlags);
	lua_stormlib_integer (L, SFileMpqFlags);

	lua_stormlib_integer (L, SFileInfoPatchChain);
	lua_stormlib_integer (L, SFileInfoFileEntry);
	lua_stormlib_integer (L, SFileInfoHashEntry);
	lua_stormlib_integer (L, SFileInfoHashIndex);
	lua_stormlib_integer (L, SFileInfoNameHash1);
	lua_stormlib_integer (L, SFileInfoNameHash2);
	lua_stormlib_integer (L, SFileInfoNameHash3);
	lua_stormlib_integer (L, SFileInfoLocale);
	lua_stormlib_integer (L, SFileInfoFileIndex);
	lua_stormlib_integer (L, SFileInfoByteOffset);
	lua_stormlib_integer (L, SFileInfoFileTime);
	lua_stormlib_integer (L, SFileInfoFileSize);
	lua_stormlib_integer (L, SFileInfoCompressedSize);
	lua_stormlib_integer (L, SFileInfoFlags);
	lua_stormlib_integer (L, SFileInfoEncryptionKey);
	lua_stormlib_integer (L, SFileInfoEncryptionKeyRaw);
	lua_stormlib_integer (L, SFileInfoCRC32);

	/* For `SFileVerifyFile ()`. */
	lua_stormlib_integer (L, SFILE_VERIFY_SECTOR_CRC);
	lua_stormlib_integer (L, SFILE_VERIFY_FILE_CRC);
	lua_stormlib_integer (L, SFILE_VERIFY_FILE_MD5);
	lua_stormlib_integer (L, SFILE_VERIFY_RAW_MD5);
	lua_stormlib_integer (L, SFILE_VERIFY_ALL);

	lua_stormlib_integer (L, VERIFY_OPEN_ERROR);
	lua_stormlib_integer (L, VERIFY_READ_ERROR);
	lua_stormlib_integer (L, VERIFY_FILE_HAS_SECTOR_CRC);
	lua_stormlib_integer (L, VERIFY_FILE_SECTOR_CRC_ERROR);
	lua_stormlib_integer (L, VERIFY_FILE_HAS_CHECKSUM);
	lua_stormlib_integer (L, VERIFY_FILE_CHECKSUM_ERROR);
	lua_stormlib_integer (L, VERIFY_FILE_HAS_MD5);
	lua_stormlib_integer (L, VERIFY_FILE_MD5_ERROR);
	lua_stormlib_integer (L, VERIFY_FILE_HAS_RAW_MD5);
	lua_stormlib_integer (L, VERIFY_FILE_ERROR_MASK);

	/* For `SFileVerifyArchive ()`. */
	lua_stormlib_integer (L, ERROR_NO_SIGNATURE);
	lua_stormlib_integer (L, ERROR_VERIFY_FAILED);
	lua_stormlib_integer (L, ERROR_WEAK_SIGNATURE_OK);
	lua_stormlib_integer (L, ERROR_WEAK_SIGNATURE_ERROR);
	lua_stormlib_integer (L, ERROR_STRONG_SIGNATURE_OK);
	lua_stormlib_integer (L, ERROR_STRONG_SIGNATURE_ERROR);

	/* For `SFileAddFileEx ()`. */
	/* MPQ_FILE_IMPLODE: Obsolete.  Use MPQ_FILE_COMPRESS. */
	lua_stormlib_integer (L, MPQ_FILE_COMPRESS);
	lua_stormlib_integer (L, MPQ_FILE_ENCRYPTED);
	lua_stormlib_integer (L, MPQ_FILE_FIX_KEY);
	lua_stormlib_integer (L, MPQ_FILE_DELETE_MARKER);
	lua_stormlib_integer (L, MPQ_FILE_SECTOR_CRC);
	lua_stormlib_integer (L, MPQ_FILE_SINGLE_UNIT);
	lua_stormlib_integer (L, MPQ_FILE_REPLACEEXISTING);

	lua_stormlib_integer (L, MPQ_COMPRESSION_HUFFMANN);
	lua_stormlib_integer (L, MPQ_COMPRESSION_ZLIB);
	lua_stormlib_integer (L, MPQ_COMPRESSION_PKWARE);
	lua_stormlib_integer (L, MPQ_COMPRESSION_BZIP2);
	lua_stormlib_integer (L, MPQ_COMPRESSION_SPARSE);
	lua_stormlib_integer (L, MPQ_COMPRESSION_ADPCM_MONO);
	lua_stormlib_integer (L, MPQ_COMPRESSION_ADPCM_STEREO);
	lua_stormlib_integer (L, MPQ_COMPRESSION_LZMA);
	lua_stormlib_integer (L, MPQ_COMPRESSION_NEXT_SAME);

	return 1;
}
