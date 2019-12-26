# Changelog

## [Unreleased]
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

[Unreleased]: https://github.com/nvs/lua-stormlib/compare/v0.1.4...HEAD
[0.1.4]: https://github.com/nvs/lua-stormlib/compare/v0.1.3...v0.1.4
[0.1.3]: https://github.com/nvs/lua-stormlib/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/nvs/lua-stormlib/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/nvs/lua-stormlib/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/nvs/lua-stormlib/releases/tag/v0.1.0
