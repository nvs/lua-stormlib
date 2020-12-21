# lua-stormlib

[![License](https://img.shields.io/github/license/nvs/lua-stormlib)](LICENSE)

## Contents

- [Overview](#overview)
- [Installation](#installation)
- [Documentation](#documentation)
- [Examples](#examples)

## Overview

**lua-stormlib** provides a [Lua] binding to [StormLib].  It attempts to
adhere to the interface provided by Lua's [I/O] library.

[Lua]: https://www.lua.org
[StormLib]: https://github.com/ladislav-zezula/StormLib
[I/O]: https://www.lua.org/manual/5.3/manual.html#6.8

## Installation

The following two dependencies must be met to utilize this library:

- [StormLib] `>= 9.23`
- [Lua] `>= 5.1` or [LuaJIT] `>= 2.0`

The easiest (and only supported) way to install **lua-stormlib** is to use
[Luarocks].  From your command line run the following:

```
luarocks install lua-stormlib
```

[Luarocks]: https://luarocks.org
[LuaJIT]: https://luajit.org

## Documentation

For details regarding functionality, please consult the source files of this
library.  Each C function that represents part of the Lua API of
**lua-stormlib** is appropriately documented.  It should be noted that an
attempt has been made to adhere to the inferface provided by Lua's [I/O]
library.  For basic references of how to use **lua-stormlib**, see the
[Examples](#examples) section.

For the majority of the functions, if an error is encountered, `nil` will be
returned, along with a `string` describing the error, and a `number`
indicating the error code.  The exceptions to this rule would be the
iterators, which always raise.

### Caveats

This is a list of known limitations of **lua-stormlib**.  Some of these may
be addressed over time.

1. **TL;DR: Your mileage may vary.**  This library has only been tested on
   Linux.  Please backup any files before use.
2. In situations where a writable archive is not closed and the Lua state is
   left open (e.g. as can be the case with `os.exit ()`), corruption of the
   archive by StormLib has been observed.  In normal usage this is quite
   rare; however, opened archives and files are garbage collected to further
   minimize this issue.
3. Neither StormLib nor this library attempt to create directories.  If that
   functionality is required, one should seek a supplemental method (e.g.
   [LuaFileSystem]).
4. StormLib considers forward slashes and backslashes to be distinct.  As
   such, care should be taken when naming or referencing files within the
   archive.
5. Only zlib compression is supported at this time.
6. Locales are not supported.  As such, the only locale ID used is `0`.
7. Setting a file's date and time is not supported.
8. Functionality primarily targets Warcraft III and its use cases.

[LuaFileSystem]: https://github.com/keplerproject/luafilesystem

## Examples

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
    -- Read-only by default.  Only modes 'r' and 'w' are supported.
    local file = mpq:open ('file.txt')
    print (file)
    file:close ()

    local file = mpq:open ('file.txt', 'r')

    file:seek ()
    file:seek ('cur', 15)
    file:seek ('end', -8)
    file:seek ('set')

    file:read ()
    file:read ('*a')
    file:read ('l', '*L', 512)

    for line in file:lines () do
    end

    file:close ()

    -- Write mode has a few caveats:
    --
    -- 1. The size must be explicitly stated.  The total amount of written
    --    data must equal this value.
    -- 2. The mode is more of a mix of write and append modes.  Existing
    --    files are truncated, but subsequent writes are forced to the then
    --    current end of file, regardless of calls to `file:seek ()`.
    -- 3. Various other functionality is limited or does not function (e.g.
    --    `file:seek ()` and `file:read ()`).
    local file = mpq:open ('file.txt', 'w', 1024)

    -- Writing more than the stated size will error.
    file:write ('text', 'more text', 5, 'and more')

    -- Cannot read from a write mode file.
    file:read (8) --> nil

    -- Get the last written position and total file size.
    file:seek ('cur')
    file:seek ('end')

    -- Any other usage is not supported and will return `nil`.
    file:seek ('set') --> nil
    file:seek ('end', -1) --> nil

    -- The total amount of written data must equal the size stated on
    -- opening the file or this will error.
    file:close ()
end

-- The archive, as well as any open files, will be garbage collected and
-- closed eventually.
--mpq:close ()
```
