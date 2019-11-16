# Changelog

## [Unreleased]
- N/A

## [0.1.1] - 2019-11-16
### Fixed
- `file:read (bytes)` did not return `nil` upon hitting the end-of-file in
  Lua 5.1.

### Removed
- `file:size ()` has been removed. Similar functionality can be achieved by
  using `file:seek ('end')`.

## [0.1.0] - 2019-11-07
- Initial versioned release.

[Unreleased]: https://github.com/nvs/lua-stormlib/compare/v0.1.1...HEAD
[0.1.1]: https://github.com/nvs/lua-stormlib/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/nvs/lua-stormlib/releases/tag/v0.1.0
