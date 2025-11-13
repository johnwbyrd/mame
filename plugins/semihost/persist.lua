-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- Persistence module for semihosting plugin

local config_module = require('semihost/config')
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
-- Returns: config object, error message (or nil on success)
function lib.load_config(logger)
	local json = require('json')
	local filename = get_config_path() .. '/' .. get_config_filename()
	local file = io.open(filename, 'r')

	if not file then
		-- No saved config, return defaults
		logger.verbose('No saved configuration found, using defaults')
		return config_module.create_default(), nil
	end

	local content = file:read('a')
	file:close()

	local loaded = json.parse(content)
	if not loaded then
		local err = string.format('Error parsing config file "%s" as JSON', filename)
		logger.error(err)
		return config_module.create_default(), err
	end

	logger.verbose('Configuration loaded from %s', filename)

	-- Create config from persisted data with validation
	local config, err = config_module.from_persistable(loaded)
	if err then
		logger.error('Error validating loaded config: %s', err)
		return config_module.create_default(), err
	end

	return config, nil
end

-- Save plugin configuration
-- Returns: success (boolean), error message (or nil on success)
function lib.save_config(config, logger)
	local path = get_config_path()
	local lfs = require('lfs')
	local attr = lfs.attributes(path)

	-- Check if path exists but is not a directory
	if attr and (attr.mode ~= 'directory') then
		local err = string.format('Cannot save config: "%s" is not a directory', path)
		logger.error(err)
		return false, err
	end

	-- Create directory if it doesn't exist
	if not attr then
		local success, err = lfs.mkdir(path)
		if not success then
			local err_msg = string.format('Cannot create config directory: %s', err or 'unknown error')
			logger.error(err_msg)
			return false, err_msg
		end
	end

	-- Convert config to persistable format
	local settings = config_module.to_persistable(config)

	-- Serialize to JSON
	local json = require('json')
	local data = json.stringify(settings, {indent = true})

	-- Write to file
	local filename = path .. '/' .. get_config_filename()
	local file = io.open(filename, 'w')
	if not file then
		local err = string.format('Cannot open config file for writing: %s', filename)
		logger.error(err)
		return false, err
	end

	file:write(data)
	file:close()

	logger.verbose('Configuration saved to %s', filename)
	return true, nil
end

return lib
