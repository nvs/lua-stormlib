local Assert = require ('stormlib._assert')
local C = require ('stormlib.core')
local File = require ('stormlib._file')

local Archive = {}
Archive.__index = Archive

local function is_closed (self)
	return not self._archive
end

local function to_archive (self)
	if is_closed (self) then
		error ('attempt to use a closed archive', 3)
	end

	return self._archive
end

local modes = {
	['r'] = function (path)
		return C.SFileOpenArchive (path, C.STREAM_FLAG_READ_ONLY)
	end,

	['r+'] = function (path)
		return C.SFileOpenArchive (path, 0)
	end,

	['w+'] = function (path)
		os.remove (path)
		return C.SFileCreateArchive (
			path, C.MPQ_CREATE_LISTFILE + C.MPQ_CREATE_ATTRIBUTES,
			C.HASH_TABLE_SIZE_MIN)
	end
}

function Archive.new (path, mode)
	Assert.argument_type (1, path, 'string')
	local new = modes [mode or 'r']
	Assert.argument (2, new, 'invalid mode')

	local self = {
		_archive = assert (new (path)),
		_names = {},
		_files = {}
	}

	return setmetatable (self, Archive)
end

function Archive:__tostring ()
	if self._archive then
		return tostring (self._archive):gsub ('Handle', 'Archive')
	else
		return 'StormLib Archive (Closed)'
	end
end

function Archive:__gc ()
	if not is_closed (self) then
		self:close ()
	end
end

function Archive:close ()
	local archive = to_archive (self)

	for name, files in pairs (self._files) do
		self._files [name] = nil

		for file in pairs (files) do
			self._names [file] = nil
			file:close ()
		end
	end

	self._archive = nil
	self._names = nil
	self._files = nil

	return C.SFileCloseArchive (archive)
end

function Archive:files (pattern, plain)
	local archive = to_archive (self)
	Assert.argument_type_or_nil (1, pattern, 'string')

	local result, message, code = C.SFileFindFirstFile (archive, '*')
	local finder, data = result, message

	return function ()
		while result do
			if data then
				local name = data.cFileName
				data = nil

				if not pattern or name:find (pattern, 1, plain) then
					return name
				end
			end

			result, message, code = C.SFileFindNextFile (finder)
			data = result
		end

		if finder then
			assert (C.SFileFindClose (finder))
		end

		if code ~= C.ERROR_NO_MORE_FILES then
			error (message, 2)
		end
	end
end

function Archive:remove (name)
	local archive = to_archive (self)
	Assert.argument_type (1, name, 'string')

	local status, message, code = C.SFileRemoveFile (archive, name)

	if not status then
		return nil, message, code
	end

	local files = self._files [name]

	if files then
		for file in pairs (files) do
			files [file] = false
		end
	end

	return true
end

local function has_file (archive, name)
	local result, message, code = C.SFileHasFile (archive, name)

	if result then
		return true
	elseif code == C.ERROR_FILE_NOT_FOUND then
		return false
	end

	error (message, 2)
end

function Archive:rename (old, new)
	local archive = to_archive (self)
	Assert.argument_type (1, old, 'string')
	Assert.argument_type (2, new, 'string')

	if old == new then
		return true
	end

	if not has_file (archive, old) then
		return nil, 'no such file or directory'
	end

	-- Use of `SFileRenameFile ()` will error if a file already exists.
	-- Ensure this will not be the case.
	self:remove (new)

	local old_files = self._files [old]
	self._files [old] = nil
	local new_files = self._files [new]

	if old_files then
		for file, status in pairs (old_files) do
			self._names [file] = new

			if new_files then
				new_files [file] = status
			end
		end

		if not new_files then
			self._files [new] = old_files
		end
	end

	return C.SFileRenameFile (archive, old, new)
end

function Archive:compact ()
	local archive = to_archive (self)
	return C.SFileCompactArchive (archive)
end

local patterns = {
	'^[rwa]%+?b?',
	'^[rwa]b?%+?'
}

local function check_mode (mode)
	for _, pattern in ipairs (patterns) do
		if mode:find (pattern) then
			return mode:gsub ('b', '')
		end
	end
end

local function read_file (archive, name)
	local file = assert (C.SFileOpenFileEx (
		archive, name, C.SFILE_OPEN_FROM_MPQ))
	local size = assert (C.SFileGetFileSize (file))
	local contents = assert (C.SFileReadFile (file, size))
	assert (C.SFileCloseFile (file))
	return contents
end

function Archive:open (name, mode)
	local archive = to_archive (self)
	Assert.argument_type (1, name, 'string')
	mode = check_mode (mode or 'r')
	Assert.argument (2, mode, 'invalid mode')
	local contents

	if has_file (archive, name) then
		if mode == 'r' or mode == 'r' or mode == 'a' then
			contents = read_file (archive, name)
		end
	elseif mode == 'r' or mode == 'r+' then
		return nil, 'no such file or directory'
	end

	local file = File.new (self, mode, contents)
	self._names [file] = name

	-- Mimic the behavior of the Lua I/O library, which allows multiple
	-- unique file handles to be opened with the same name.
	local files = self._files [name] or {}
	self._files [name] = files
	files [file] = true

	return file
end

local function check_limit (archive)
	local info = C.SFileGetFileInfo
	local count = assert (info (archive, C.SFileMpqNumberOfFiles))
	local limit = assert (info (archive, C.SFileMpqMaxFileCount))

	-- Unless flushed, certain files (i.e. the listfile, attributes, and
	-- signature) do not appear in the count.  Err on the side of caution.
	count = count + 3

	if count >= limit then
		-- StormLib always sets the limit to a power of two.  Incrementing
		-- pushes to the next one.
		assert (C.SFileSetMaxFileCount (archive, limit + 1))
	end
end

local function write_file (archive, name, contents)
	local file = assert (C.SFileCreateFile (
		archive, name, 0, #contents, 0,
		C.MPQ_FILE_REPLACEEXISTING + C.MPQ_FILE_COMPRESS))
	assert (C.SFileWriteFile (file, contents, C.MPQ_COMPRESSION_ZLIB))
	assert (C.SFileFinishFile (file))
end

function Archive:_close_file (file, handle, mode)
	local archive = to_archive (self)
	local name = self._names [file]
	local files = self._files [name]

	if mode ~= 'r' and files [file] then
		handle:seek ('set')
		local contents = handle:read ('*a')

		check_limit (archive)
		write_file (archive, name, contents)
	end

	self._names [file] = nil
	files [file] = nil

	if next (files) == nil then
		self._files [name] = nil
	end
end

return Archive
