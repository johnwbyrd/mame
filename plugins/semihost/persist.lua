-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- Persistence module for semihosting plugin

local lib = {}

-- Get plugin configuration directory path
local function get_config_path()
	local homepath = manager.options.entries.homepath:value():match('([^;]+)')
	return homepath .. '/semihost'
end

-- Get global config filename (not per-game)
local function get_config_filename()
	return 'semihost.json'
end

-- Load plugin configuration
function lib:load_config()
	local config = {
		base_addr = nil,  -- Will use auto-detect if nil
		sandbox_dir = '', -- Empty means auto
		logging = false
	}

	local json = require('json')
	local filename = get_config_path() .. '/' .. get_config_filename()
	local file = io.open(filename, 'r')

	if not file then
		-- No saved config, return defaults
		return config
	end

	local loaded = json.parse(file:read('a'))
	file:close()

	if not loaded then
		emu.print_error(string.format('[SEMIHOST] Error parsing config file "%s" as JSON', filename))
		return config
	end

	-- Merge loaded settings with defaults
	if loaded.base_addr ~= nil then
		config.base_addr = loaded.base_addr
	end
	if loaded.sandbox_dir ~= nil then
		config.sandbox_dir = loaded.sandbox_dir
	end
	if loaded.logging ~= nil then
		config.logging = loaded.logging
	end

	return config
end

-- Save plugin configuration
function lib:save_config(state)
	local path = get_config_path()
	local lfs = require('lfs')
	local attr = lfs.attributes(path)

	-- Check if path exists but is not a directory
	if attr and (attr.mode ~= 'directory') then
		emu.print_error(string.format('[SEMIHOST] Cannot save config: "%s" is not a directory', path))
		return false
	end

	-- Create directory if it doesn't exist
	if not attr then
		local success, err = lfs.mkdir(path)
		if not success then
			emu.print_error(string.format('[SEMIHOST] Cannot create config directory: %s', err or 'unknown error'))
			return false
		end
	end

	-- Prepare settings to save
	local settings = {
		base_addr = state.base_addr,
		sandbox_dir = state.sandbox_dir,
		logging = state.logging
	}

	-- Serialize to JSON
	local json = require('json')
	local data = json.stringify(settings, {indent = true})

	-- Write to file
	local filename = path .. '/' .. get_config_filename()
	local file = io.open(filename, 'w')
	if not file then
		emu.print_error(string.format('[SEMIHOST] Cannot open config file for writing: %s', filename))
		return false
	end

	file:write(data)
	file:close()

	return true
end

return lib
