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
	'lua >= 5.1, < 5.5',
}

external_dependencies = {
	platforms = {
		linux = {
			STORM = {
				library = 'storm'
			}
		},
		windows = {
			STORM = {
				library = 'stormlib'
			}
		}
	}
}

build = {
	type = 'builtin',
	modules = {
		storm = {
			sources = {
				'src/init.c',
				'src/common.c',
				'src/file.c',
				'src/finder.c',
				'src/handles.c',
				'src/mpq.c'
			},
			incdirs = {
				'lib/compat-5.3/c-api',
				'lib/stormlib/src',
				'$(STORM_INCDIR)'
			},
			libdirs = {
				'$(STORM_LIBDIR)'
			}
		}
	},
	platforms = {
		linux = {
			modules = {
				storm = {
					libraries = {
						'storm'
					}
				}
			}
		},
		windows = {
			modules = {
				storm = {
					libraries = {
						'stormlib'
					}
				}
			}
		}
	}
}
