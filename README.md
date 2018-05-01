# lua-stormlib

[![License](https://img.shields.io/github/license/nvs/lua-stormlib.svg)](LICENSE)

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

- [StormLib] `>= 9.22`
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
library (or see the [Examples](#examples) section).  It should be noted
that an attempt has been made to adhere to the inferface provide by Lua's
[I/O] library.

For the majority of the functions, if an error is encountered, `nil` will be
returned, along with a `string` describing the error, and a `number`
indicating the error code.  The exceptions to this rule would be the
iterators, which always raise.

### Caveats

1. Neither StormLib or this library attempt to create directories.  If that
   functionality is required, one should seek an alternative method (e.g.
   [LuaFileSystem]).
2. StormLib considers forward slashes and backslashes to be distinct.  As
   such, care should be taken when naming or referencing files within the
   archive.
3. **TL;DR: Your mileage may vary.**  This library has only been tested on
   Linux (and even then, not very thorougly).  Please backup any files
   before use.

[LuaFileSystem]: https://github.com/keplerproject/luafilesystem

## Examples

``` lua
local stormlib = require ('stormlib')

-- Read-only by default.  Only modes 'r' and 'r+' are supported.
local mpq = stormlib.open ('example.w3x')
print (mpq)
mpq:close ()

local mpq = stormlib.open ('example.w3x', 'r')
mpq:close ()

-- Update mode.  Existing data is preserved.
local mpq = stormlib.open ('example.w3x', 'r+')

-- The current number of files.
mpq:count ()

-- The maximum number of files.
mpq:limit ()

-- Whether the archive contains the named file.
mpq:has ('file.txt')

-- Iterate through a list of all file names.  Can use wildcards '*' (to
-- match any number of characters) and '?' (to match any single character).
-- By default, '*' is used.
for name in mpq:list () do
    -- All files in archive.
end

for name in mpq:list ('war?map.*') do
    -- All matching files.
end

-- If the archive is full, an attempt to automatically increase the limit
-- will be made.
mpq:add ('path/to/a/file.txt') -- Adds as 'path/to/a/file.txt'.
mpq:add ('path/to/a/file.txt', 'file.txt') -- Adds as 'file.txt'.

-- Destination path must be explicit.  Does not create directories, nor does
-- it place a file within a specified directory.
mpq:extract ('file.txt', 'path/to/a/file.txt')

mpq:remove ('file.txt')
mpq:rename ('file.txt', 'other-file.txt')

-- Rebuilds the archive, attempting to save space.
mpq:compact ()

do
    -- Read-only by default.  Only modes 'r' and 'w' are supported.
    local file = mpq:open ('file.txt')
    print (file)
    file:close ()

    local file = mpq:open ('file.txt', 'r')

    file:size ()

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

    -- The size must be explicitly stated for write enabled files.  Write
    -- mode is a cross between write and append modes, in that existing
    -- files are truncated, but writing can only occur at the end of the
    -- file.  Also note that if the archive is full, an attempt to
    -- automatically increase the limit will be made.
    local file = mpq:open ('file.txt', 'w', 1024)

    -- Writing more than the stated size will error, and the file will not
    -- be finalized.
    file:write ('text', 'more text', 5, 'and more')

    -- The total amount of written data must equal the size stated on
    -- opening the file or this will error.
    file:close ()
end

mpq:close ()
```
