local File = {}
File.__index = File

local function is_closed (self)
	return not self._file
end

local function to_file (self)
	if is_closed (self) then
		error ('attempt to use a closed file', 3)
	end

	return self._file
end

function File.new (archive, mode, contents)
	local file = assert (io.tmpfile ())

	if contents then
		file:write (contents)
		file:seek ('set')
	end

	local self = {
		_archive = archive,
		_file = file,
		_mode = mode
	}

	return setmetatable (self, File)
end

function File:__tostring ()
	if self._file then
		return tostring (self._file):gsub ('file', 'StormLib File')
	else
		return 'StormLib File (Closed)'
	end
end

function File:__gc ()
	if self._file then
		self:close ()
	end
end

function File:close ()
	local file = to_file (self)
	local archive = self._archive

	self._file = nil
	self._archive = nil

	archive:_close_file (self, file, self._mode)
	return file:close ()
end

function File:flush (...)
	local file = to_file (self)
	return file:flush (...)
end

function File:lines (...)
	local file = to_file (self)

	if self._mode == 'w' or self._mode == 'a' then
		error ('bad file descriptor', 2)
	end

	return file:lines (...)
end

function File:read (...)
	local file = to_file (self)

	if self._mode == 'w' or self._mode == 'a' then
		error ('bad file descriptor', 2)
	end

	return file:read (...)
end

function File:seek (...)
	local file = to_file (self)
	return file:seek (...)
end

function File:setvbuf (...)
	local file = to_file (self)
	return file:setvbuf (...)
end

function File:write (...)
	local file = to_file (self)

	if self._mode == 'r' then
		error ('bad file descriptor', 2)
	elseif self._mode == 'a' or self._mode == 'a+' then
		self._file:seek ('end')
	end

	local status, result, code = file:write (...)

	if status then
		return self
	else
		return status, result, code
	end
end

return File
