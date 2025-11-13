-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- Configuration management module for semihosting plugin

local constants = require('semihost/constants')
local lib = {}

-- Validate a base address
local function validate_base_addr(addr)
	if type(addr) ~= 'number' then
		return nil, 'base_addr must be a number'
	end

	if addr < 0 then
		return nil, 'base_addr must be non-negative'
	end

	-- Check if it's a reasonable address (within 32-bit range)
	if addr > 0xFFFFFFFF then
		return nil, 'base_addr exceeds 32-bit address space'
	end

	return addr, nil
end

-- Validate sandbox directory path
local function validate_sandbox_dir(dir)
	if type(dir) ~= 'string' then
		return nil, 'sandbox_dir must be a string'
	end

	-- Empty string is valid (means auto-detect)
	if dir == '' then
		return dir, nil
	end

	-- Check for security issues
	if dir:match('%.%.') then
		return nil, 'sandbox_dir cannot contain ".."'
	end

	return dir, nil
end

-- Validate logging flag
local function validate_logging(logging)
	if type(logging) ~= 'boolean' then
		return nil, 'logging must be boolean'
	end
	return logging, nil
end

-- Create a new configuration object with defaults
function lib.create_default()
	return {
		base_addr = constants.DEFAULT_BASE_ADDR,
		sandbox_dir = constants.DEFAULT_SANDBOX_DIR,
		logging_enabled = constants.DEFAULT_LOGGING,
	}
end

-- Create configuration from user values with validation
function lib.create(values)
	local config = lib.create_default()
	local errors = {}

	if values.base_addr ~= nil then
		local validated, err = validate_base_addr(values.base_addr)
		if err then
			table.insert(errors, err)
		else
			config.base_addr = validated
		end
	end

	if values.sandbox_dir ~= nil then
		local validated, err = validate_sandbox_dir(values.sandbox_dir)
		if err then
			table.insert(errors, err)
		else
			config.sandbox_dir = validated
		end
	end

	if values.logging_enabled ~= nil then
		local validated, err = validate_logging(values.logging_enabled)
		if err then
			table.insert(errors, err)
		else
			config.logging_enabled = validated
		end
	end

	-- Backwards compatibility: accept 'logging' as alias for 'logging_enabled'
	if values.logging ~= nil and values.logging_enabled == nil then
		local validated, err = validate_logging(values.logging)
		if err then
			table.insert(errors, err)
		else
			config.logging_enabled = validated
		end
	end

	if #errors > 0 then
		return nil, table.concat(errors, '; ')
	end

	return config, nil
end

-- Update configuration with new values
function lib.update(config, changes)
	local new_values = {}

	if changes.base_addr ~= nil then
		new_values.base_addr = changes.base_addr
	else
		new_values.base_addr = config.base_addr
	end

	if changes.sandbox_dir ~= nil then
		new_values.sandbox_dir = changes.sandbox_dir
	else
		new_values.sandbox_dir = config.sandbox_dir
	end

	if changes.logging_enabled ~= nil then
		new_values.logging_enabled = changes.logging_enabled
	else
		new_values.logging_enabled = config.logging_enabled
	end

	return lib.create(new_values)
end

-- Convert config to persistable format (for JSON serialization)
function lib.to_persistable(config)
	return {
		base_addr = config.base_addr,
		sandbox_dir = config.sandbox_dir,
		logging = config.logging_enabled,  -- Use 'logging' for backwards compatibility
	}
end

-- Create config from persisted format
function lib.from_persistable(data)
	return lib.create({
		base_addr = data.base_addr,
		sandbox_dir = data.sandbox_dir,
		logging = data.logging,  -- Accept 'logging' from old format
		logging_enabled = data.logging_enabled,  -- Accept new format
	})
end

-- Check if base address is set to auto-detect
function lib.is_auto_detect(config)
	return config.base_addr == constants.DEFAULT_BASE_ADDR
end

-- Get common base addresses for cycling in menu
function lib.get_common_addresses()
	return constants.COMMON_BASE_ADDRESSES
end

return lib
