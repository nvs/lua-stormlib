local Assert = {}

local function error_argument (id, message, level)
	message = 'bad argument'
		.. ' ' .. (type (id) == 'number' and '#%d' or 'for \'%s\'')
		.. ' (' .. (message or 'assertion failed!') .. ')'

	error (message:format (id), level + 1)
end

local function error_argument_type (id, expected, received, level)
	error_argument (
		id, ('%s expected, got %s'):format (expected, received), level + 1)
end

function Assert.argument (id, value, message)
	if value then
		return
	end

	Assert.argument_type_or_nil (3, message, 'string')
	error_argument (id, message, 2)
end

function Assert.argument_type (id, value, expected)
	local received = type (value)

	if received == expected then
		return
	end

	error_argument_type (id, expected, received, 3)
end

function Assert.argument_type_or_nil (id, value, expected)
	if value == nil then
		return
	end

	local received = type (value)

	if received == expected then
		return
	end

	error_argument_type (id, expected, received, 3)
end

return Assert
