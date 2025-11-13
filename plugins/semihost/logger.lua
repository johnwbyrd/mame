-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- Centralized logging module for semihosting plugin

local constants = require('semihost/constants')
local lib = {}

-- Create a new logger instance
function lib.create(config)
	local self = {
		prefix = '[SEMIHOST]',
		logging_enabled = config.logging_enabled or false,
		min_level = constants.LOG_LEVEL.VERBOSE,  -- Default minimum level for optional logs
	}

	-- Update logging configuration
	function self.set_logging_enabled(enabled)
		self.logging_enabled = enabled
	end

	-- Update minimum log level
	function self.set_min_level(level)
		self.min_level = level
	end

	-- Internal logging function
	local function log(level, level_name, emu_print_fn, message, ...)
		-- Always show ERROR, WARNING, and INFO
		if level <= constants.LOG_LEVEL.INFO then
			local formatted = string.format(message, ...)
			emu_print_fn(string.format("%s %s", self.prefix, formatted))
			return
		end

		-- For VERBOSE and above, check if logging is enabled
		if not self.logging_enabled then
			return
		end

		-- Check if level is sufficient
		if level > self.min_level then
			return
		end

		local formatted = string.format(message, ...)
		emu_print_fn(string.format("%s %s", self.prefix, formatted))
	end

	-- Public logging functions
	function self.error(message, ...)
		log(constants.LOG_LEVEL.ERROR, 'ERROR', emu.print_error, message, ...)
	end

	function self.warning(message, ...)
		log(constants.LOG_LEVEL.WARNING, 'WARNING', emu.print_warning, message, ...)
	end

	function self.info(message, ...)
		log(constants.LOG_LEVEL.INFO, 'INFO', emu.print_info, message, ...)
	end

	function self.verbose(message, ...)
		log(constants.LOG_LEVEL.VERBOSE, 'VERBOSE', emu.print_verbose, message, ...)
	end

	function self.debug(message, ...)
		log(constants.LOG_LEVEL.DEBUG, 'DEBUG', emu.print_debug, message, ...)
	end

	-- Guest output (stdout/stderr) - separate from diagnostic logging
	-- This always outputs regardless of logging settings
	function self.guest_output(text)
		io.write(text)
		io.flush()
	end

	-- Guest output with explicit newline
	function self.guest_writeln(text)
		io.write(text)
		io.write('\n')
		io.flush()
	end

	return self
end

return lib
