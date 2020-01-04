rockspec_format = "3.0"

package = "lua-stormlib"
version = "scm-0"

description = {
	summary = 'A Lua binding to StormLib',
	homepage = 'https://github.com/nvs/lua-stormlib',
	license = 'MIT'
}

source = {
	url = 'git+https://github.com/nvs/lua-stormlib.git'
}

dependencies = {
	'lua >= 5.1, < 5.4',
}

build = {
   type = "builtin",
   modules = {
		['stormlib'] = {
			sources = {
				'src/init.c',
				'src/common.c',
				'src/file.c',
				'src/finder.c',
				'src/handles.c',
				'src/mpq.c',
				'lib/compat-5.3/c-api/compat-5.3.c',
				'lib/stormlib/src/SBaseCommon.c',
				'lib/stormlib/src/SBaseFileTable.c',
			},
			libraries = {
				'storm'
			},
			incdirs = {
				'lib/compat-5.3/c-api',
				'lib/stormlib/src'
			}
		}
   }
}
