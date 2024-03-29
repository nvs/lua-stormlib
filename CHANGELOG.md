# Changelog

## [Unreleased]
- N/A

## [0.3.1] - 2022-07-04
### Fixed
- Resolve `archive:close ()` error when files are left open.

## [0.3.0] - 2022-05-28
### Changed
- Introduce two distinct API: Core and Lua.
    - Core API more closely follows StormLib's design.
    - Lua API continues to follow Lua's I/O design.

## [0.2.3] - 2020-12-20
### Changed
- Require StormLib 9.23.

## [0.2.2] - 2020-08-06
### Changed
- Add Lua 5.4 support.

## [0.2.1] - 2020-01-03
### Changed
- `file:seek ()` behavior for writable files has changed:
  - The file position does not change.
  - Only 'cur' and 'end are accepted, and return the last write position and
    end of file, repesctively.
  - The offset, if provided, must be zero.

### Fixed
- Automatic increasing of an archive's file limits now works properly when
  there are open files.

## [0.2.0] - 2019-12-27
### Changed
- `mpq:files ()` now takes an optional Lua pattern to refine the results.
  It also supports plain text search as well.

### Fixed
- Resolve `file:read ()` returning `nil` upon reaching end-of-file.

### Removed
- The following functions have been removed:
  - `mpq:has ()`
  - `mpq:add ()`
  - `mpq:extract ()`

## [0.1.6] - 2019-12-26
### Changed
- `mpq:list ()` has been renamed to `mpq:files ()`, and no longer takes a
  mask argument.

## [0.1.5] - 2019-12-26
### Fixed
- Address inability to bind against StormLib > `9.22`.

## [0.1.4] - 2019-12-24
### Changed
- Accept `b` with `mpq:open ()`.  It does nothing, but maintains consistency
  with the Lua I/O API.

## [0.1.3] - 2019-12-21
### Fixed
- Properly validate modes on `stormlib.open ()`: `w` was being accepted,
  when it should not have been.

## [0.1.2] - 2019-11-8
### Fixed
- Properly fix `file:read (bytes)` upon hitting end-of-file in Lua 5.1.

## [0.1.1] - 2019-11-16
### Fixed
- `file:read (bytes)` did not return `nil` upon hitting the end-of-file in
  Lua 5.1.

### Removed
- `file:size ()` has been removed. Similar functionality can be achieved by
  using `file:seek ('end')`.

## [0.1.0] - 2019-11-07
- Initial versioned release.

[Unreleased]: https://github.com/nvs/lua-stormlib/compare/v0.3.1...HEAD
[0.3.1]: https://github.com/nvs/lua-stormlib/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/nvs/lua-stormlib/compare/v0.2.3...v0.3.0
[0.2.3]: https://github.com/nvs/lua-stormlib/compare/v0.2.2...v0.2.3
[0.2.2]: https://github.com/nvs/lua-stormlib/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/nvs/lua-stormlib/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/nvs/lua-stormlib/compare/v0.1.6...v0.2.0
[0.1.6]: https://github.com/nvs/lua-stormlib/compare/v0.1.5...v0.1.6
[0.1.5]: https://github.com/nvs/lua-stormlib/compare/v0.1.4...v0.1.5
[0.1.4]: https://github.com/nvs/lua-stormlib/compare/v0.1.3...v0.1.4
[0.1.3]: https://github.com/nvs/lua-stormlib/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/nvs/lua-stormlib/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/nvs/lua-stormlib/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/nvs/lua-stormlib/releases/tag/v0.1.0
