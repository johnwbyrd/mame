-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- File system sandbox management for semihosting plugin

local constants = require('semihost/constants')
local lib = {}

-- Create a sandbox manager
function lib.create(config, logger)
	local self = {
		sandbox_dir = config.sandbox_dir,
		logger = logger,
	}

	-- Get the effective sandbox directory
	function self.get_sandbox_dir()
		return self.sandbox_dir
	end

	-- Set sandbox directory
	function self.set_sandbox_dir(dir)
		self.sandbox_dir = dir
	end

	-- Sanitize and validate file path within sandbox
	-- Returns: sanitized_path, error_code
	function self.sanitize_path(path)
		-- Remove any .. or absolute path components for security
		if path:match('^/') or path:match('%.%.') then
			self.logger.verbose('Path blocked (security): %s', path)
			return nil, constants.ERRNO.EACCES
		end

		-- If no sandbox is set, return path as-is (relative to current directory)
		if self.sandbox_dir == '' then
			return path, constants.ERRNO.SUCCESS
		end

		-- Build full path within sandbox
		local full_path = self.sandbox_dir .. '/' .. path
		return full_path, constants.ERRNO.SUCCESS
	end

	-- Ensure sandbox directory exists and is valid
	-- Returns: success (boolean), error_message (string or nil)
	function self.ensure_sandbox_exists()
		if self.sandbox_dir == '' then
			-- No sandbox directory configured
			return true, nil
		end

		local lfs = require('lfs')
		local attr = lfs.attributes(self.sandbox_dir)

		-- Check if path exists but is not a directory
		if attr and (attr.mode ~= 'directory') then
			local err = string.format('Path exists but is not a directory: %s', self.sandbox_dir)
			self.logger.error(err)
			return false, err
		end

		-- Directory already exists
		if attr then
			self.logger.verbose('Using existing sandbox directory: %s', self.sandbox_dir)
			return true, nil
		end

		-- Try to create the directory
		self.logger.info('Creating sandbox directory: %s', self.sandbox_dir)
		local success, err = lfs.mkdir(self.sandbox_dir)

		if not success then
			local err_msg = string.format('Cannot create sandbox directory: %s', err or 'unknown error')
			self.logger.error(err_msg)
			return false, err_msg
		end

		self.logger.info('Sandbox directory created successfully')
		return true, nil
	end

	-- Initialize sandbox for a given configuration
	-- If sandbox_dir is empty, auto-detect and set it
	-- Returns: updated_sandbox_dir, success, error
	function self.initialize_auto()
		if self.sandbox_dir ~= '' then
			-- Already configured, just ensure it exists
			local success, err = self.ensure_sandbox_exists()
			return self.sandbox_dir, success, err
		end

		-- Auto-detect: use homepath/semihost
		local homepath = manager.options.entries.homepath:value():match('([^;]+)')
		self.sandbox_dir = homepath .. '/semihost'
		self.logger.info('Auto-configured sandbox directory: %s', self.sandbox_dir)

		local success, err = self.ensure_sandbox_exists()
		return self.sandbox_dir, success, err
	end

	return self
end

return lib
