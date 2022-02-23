# lua-stormlib

**lua-stormlib** provides a [Lua] binding to [StormLib].  It provides two
interfaces: one in the style of [Lua's I/O] library and one that is
similar to the [StormLib API].

## Contents

- [Installation](#installation)
- [Usage](#usage)
- [Lua API](#lua-api)
- [Core API](#core-api)

## Installation

The following two dependencies must be met to utilize this library:

- [StormLib] `>= 9.23`
- [Lua] `>= 5.1` or [LuaJIT] `>= 2.0`

The easiest (and only supported) way to install **lua-stormlib** is to use
[Luarocks].  From your command line run the following:

```
luarocks install lua-stormlib
```

Note that Luarocks does not support static libraries.  As such, pass
`-DBUILD_SHARED_LIBS=on` to cmake when building StormLib.

## Usage

There are two ways to utilize this library:

1. The easiest is to make use of the Lua API of **lua-stormlib**.  It should
   be noted that an attempt has been made to adhere to the interface
   provided by [Lua's I/O] library.  For details, see the [Lua
   API](#lua-api).
2. For users that want an experience that is closer to using StormLib
   directly, the Core API of **lua-stormlib** can be leveraged.  An
   attempt has been made to mirror the StormLib API, within reason.  For
   details, see the [Core API](#core-api).

### Caveats

This is a list of known limitations of **lua-stormlib**.  Some of these may
be addressed over time.

1. **TL;DR: Your mileage may vary.**  Please backup any files before use.
2. This library has only been tested on Linux and WSL.  No testing has been
   done on Windows.  Nor are there any plans to.  Pull requests to ensure
   proper Windows functionality are welcome.
3. Not every function provided by StormLib is fully supported yet within
   the Core API.  In particular, `SFileGetFileInfo ()` has behavior that has
   not been implemented yet.

## Lua API

The Lua API of **lua-stormlib** is written entirely in Lua using the Core
API of this project.  As such, behaviors mentioned there apply here as
well.  This API mirrors [Lua's I/O] library, and should feel very
comfortable to those familiar with Lua.  For a full list of supported
functions, consult the source.  Here are some examples:

``` lua
local stormlib = require ('stormlib')

-- Read-only by default.  Only modes 'r', 'w+', and 'r+' are supported.
local mpq = stormlib.open ('example.w3x')
print (mpq)

-- This will close the archive, as well as any of its open files.
mpq:close ()

local mpq = stormlib.open ('example.w3x', 'r')
mpq:close ()

-- Update mode.  Existing data is preserved.
local mpq = stormlib.open ('example.w3x', 'r+')
mpq:close ()

-- Update mode.  Existing data is erased.  This can be used to create a new
-- archive.
local mpq = stormlib.open ('example.w3x', 'w+')

-- Iterate through a list of all file names.
for name in mpq:files () do
    -- All files in archive.
end

-- Can take a Lua pattern to refine the results.
for name in mpq:files ('^war.map.*') do
    -- All matching files.
end

-- Can also take a plain string.
for name in mpq:files ('.txt', true) do
    -- All files that contain the matching string.
end

mpq:remove ('file.txt')
mpq:rename ('file.txt', 'other-file.txt')

-- Rebuilds the archive, attempting to save space.  Removed, renamed, and
-- replaced files will remain in the archive until it is compacted.  Note
-- that this has the potential to be a costly operation on some archives.
mpq:compact ()

do
    -- All Lua I/O for file handle objects should be supported.
    local file = mpq:open ('file.txt')
    print (file)
    file:close ()

    local file = mpq:open ('file.txt', 'r')

    file:seek ()
    file:seek ('cur', 15)
    file:seek ('end', -8)
    file:seek ('set')

    file:read ()
    file:read ('l', '*L', 512)
    file:read ('*a')

    for line in file:lines () do
    end

    file:close ()

    local file = mpq:open ('file.txt', 'w')
    file:write ('text', 'more text', 5, 'and more')

    -- Get the last written position and total file size.
    file:seek ('cur')
    file:seek ('end')

    file:close ()
end

mpq:close ()
```

## Core API

The Core API of **lua-stormlib** attempts to mirror [StormLib], down to
function name and argument order.  Consistency with StormLib is prioritized
over ease of use within Lua.  Referencing the [StormLib API] and the
[StormLib.h] should prove quite useful.  However, there are a few
differences:

- Opened handles are automatically closed and garbage collected. Still, it
  is recommended to manually manage such tasks.
- The handling of `NULL` in various situations (e.g. certain strings and
  function callbacks): In these cases, passing `nil` or omitting the
  argument will work the same as passing `NULL`.
- In cases where StormLib returns a pointer, the Core API will simply return
  a `table`.  Consider this to be a snapshot at that exact moment.

For nearly all functions, if an error is encounted then `nil` will be
returned, along with a `string` describing the error, and, if applicable, a
`number` indicating the error code.  For details, please consult the source.
Here are some examples:

``` lua
local C = require ('stormlib.core')

local path = '...'
local archive = C.SFileOpenArchive (path, C.STREAM_FLAG_READ_ONLY)

-- Print a list of all files in the archive.
do
    local result, message, code = C.SFileFindFirstFile (archive, '*')
    local finder, data = result, message

    while result do
        if data then
            print (data.cFileName)
        end

        result, message, code = C.SFileFindNextFile (finder)
        data = result
    end

    if finder then
        C.SFileFindClose (finder)
    end

    if code ~= C.ERROR_NO_MORE_FILES then
         error (message)
    end
end

-- Read and print the contents of a single file.
do
    local name = '...'
    local file = C.SFileOpenFileEx (archive, name, C.SFILE_OPEN_FROM_MPQ)
    local size = C.SFileGetFileSize (file)
    local contents = C.SFileReadFile (file, size)
    C.SFileCloseFile (file)

    print (contents)
end

C.SFileCloseArchive (archive)
```

[Lua]: https://www.lua.org
[Lua's I/O]: https://www.lua.org/manual/5.4/manual.html#6.8
[StormLib]: https://github.com/ladislav-zezula/StormLib
[StormLib API]: http://www.zezula.net/en/mpq/stormlib.html
[StormLib.h]: https://github.com/ladislav-zezula/StormLib/blob/master/src/StormLib.h
[Luarocks]: https://luarocks.org
[LuaJIT]: https://luajit.org
