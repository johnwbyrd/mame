-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- RIFF-based semihosting device plugin

local exports = {
	name = 'semihost',
	version = '1.0.0',
	description = 'RIFF-based semihosting device',
	license = 'BSD-3-Clause',
	author = { name = 'MAME Team' }
}

local semihost = exports

-- Require modules
local constants = require('semihost/constants')
local riff = require('semihost/riff_parser')
local syscalls_module = require('semihost/syscalls')
local menu_module = require('semihost/menu')
local persist = require('semihost/persist')
local logger_module = require('semihost/logger')
local config_module = require('semihost/config')
local sandbox_module = require('semihost/sandbox')

local reset_subscription, stop_subscription, frame_subscription

-- Plugin state (separate configuration from runtime state)
local config = nil  -- Configuration object (immutable)
local logger = nil  -- Logger instance
local sandbox = nil -- Sandbox manager
local menu = nil    -- Menu handler

-- Runtime state
local runtime = {
	installed = false,
	parsed_config = nil, -- Current CNFG chunk data from RIFF
	syscall_handler = nil, -- Syscall executor
	memory = nil, -- Current address space
}

-- Determine default base address based on CPU address width
local function get_default_base_addr()
	if not manager.machine.devices[':maincpu'] then
		return constants.DEFAULT_BASE_ADDR
	end

	local cpu = manager.machine.devices[':maincpu']
	if not cpu.spaces or not cpu.spaces['program'] then
		return constants.DEFAULT_BASE_ADDR
	end

	local addr_space = cpu.spaces['program']
	local addr_bits = addr_space.addr_width

	-- For 8-bit and 16-bit systems, use lower address
	if addr_bits <= 16 then
		return 0xFC00
	else
		return constants.DEFAULT_BASE_ADDR
	end
end

-- Read bytes from memory into a table
local function read_memory_bytes(memory, addr, count)
	local bytes = {}
	for i = 0, count - 1 do
		bytes[i] = memory:read_u8(addr + i)
	end
	return bytes
end

-- Write bytes from table to memory
local function write_memory_bytes(memory, addr, bytes)
	for i = 0, #bytes do
		if bytes[i] then
			memory:write_u8(addr + i, bytes[i])
		end
	end
end

-- Update configuration (called by menu or persistence)
local function update_config(changes)
	-- Create new config with changes
	local new_config, err = config_module.update(config, changes)
	if err then
		logger.error('Failed to update config: %s', err)
		return false
	end

	config = new_config

	-- Update logger if logging changed
	if changes.logging_enabled ~= nil then
		logger.set_logging_enabled(config.logging_enabled)
	end

	-- Update sandbox if sandbox_dir changed
	if changes.sandbox_dir ~= nil then
		sandbox.set_sandbox_dir(config.sandbox_dir)
	end

	-- Update menu with new config
	if menu then
		menu.update_config(config)
	end

	-- Save configuration
	save_config()

	return true
end

-- Process semihosting request
local function process_request()
	if not runtime.installed or not runtime.memory then
		return
	end

	-- Read RIFF structure from base address
	local bytes = read_memory_bytes(runtime.memory, config.base_addr, constants.RIFF_BUFFER_SIZE)

	-- Parse RIFF chunks
	local chunks, err = riff.parse_chunks(bytes)
	if not chunks then
		logger.error('Failed to parse RIFF: %s', err)
		return
	end

	-- Find CNFG chunk and update configuration
	local cnfg_chunk = riff.find_chunk(chunks, 'CNFG')
	if cnfg_chunk then
		local cnfg, err = riff.parse_cnfg(bytes, cnfg_chunk)
		if cnfg then
			runtime.parsed_config = cnfg

			logger.verbose('Configuration: word_size=%d, ptr_size=%d, endianness=%d',
				cnfg.word_size, cnfg.ptr_size, cnfg.endianness)

			-- Recreate syscall executor with new config
			local syscall_config = {
				word_size = cnfg.word_size,
				ptr_size = cnfg.ptr_size,
				endianness = cnfg.endianness,
			}
			runtime.syscall_handler = syscalls_module.create(syscall_config, logger, sandbox)
		else
			logger.error('Invalid CNFG chunk: %s', err)
			return
		end
	end

	-- Find CALL chunk
	local call_chunk = riff.find_chunk(chunks, 'CALL')
	if not call_chunk or not runtime.parsed_config or not runtime.syscall_handler then
		return
	end

	-- Parse CALL chunk
	local call, err = riff.parse_call(bytes, call_chunk, runtime.parsed_config.ptr_size, runtime.parsed_config.endianness)
	if not call then
		logger.error('Invalid CALL chunk: %s', err)
		return
	end

	logger.verbose('Syscall 0x%02X, arg_ptr=0x%X', call.opcode, call.arg_ptr)

	-- Execute syscall
	local result, errno = runtime.syscall_handler.execute(call.opcode, call.arg_ptr, runtime.memory, riff)

	logger.verbose('Result: %d, errno: %d', result, errno)

	-- Create RETN chunk
	local retn_bytes = riff.create_retn(result, errno, runtime.parsed_config.word_size, runtime.parsed_config.endianness)

	-- Write RETN chunk back to memory (replacing CALL chunk)
	local retn_addr = config.base_addr + call_chunk.offset - 8 -- Subtract header size
	write_memory_bytes(runtime.memory, retn_addr, retn_bytes)
end

-- Install memory monitoring
local function install_semihost()
	if runtime.installed then
		return
	end

	-- Check if machine is available
	if not manager.machine or not manager.machine.devices then
		return
	end

	local cpu = manager.machine.devices[':maincpu']
	if not cpu then
		-- CPU not available yet, will retry on next reset/frame
		return
	end

	if not cpu.spaces or not cpu.spaces['program'] then
		logger.warning('Main CPU has no program space')
		return
	end

	runtime.memory = cpu.spaces['program']

	-- Update default base address if using default
	if config_module.is_auto_detect(config) then
		local detected_addr = get_default_base_addr()
		if detected_addr ~= config.base_addr then
			update_config({base_addr = detected_addr})
		end
	end

	logger.verbose('Installing at base=0x%X', config.base_addr)

	-- Initialize sandbox directory (auto-detect if needed)
	local updated_sandbox_dir, success, err = sandbox.initialize_auto()
	if not success then
		logger.warning('Sandbox initialization issue: %s', err or 'unknown')
	elseif updated_sandbox_dir ~= config.sandbox_dir then
		-- Update config with auto-detected sandbox dir (don't save yet, wait for stop)
		config = config_module.update(config, {sandbox_dir = updated_sandbox_dir})
		sandbox.set_sandbox_dir(config.sandbox_dir)
	end

	-- Install write tap on trigger address
	-- Note: MAME Lua doesn't have direct write tap support, so we'll use periodic checking
	runtime.installed = true

	logger.verbose('Semihosting device ready')
end

-- Uninstall memory monitoring
local function uninstall_semihost()
	if not runtime.installed then
		return
	end

	logger.verbose('Uninstalling semihosting device')

	-- Clean up open files
	if runtime.syscall_handler then
		runtime.syscall_handler.cleanup()
	end

	runtime.installed = false
	runtime.memory = nil
	runtime.parsed_config = nil
	runtime.syscall_handler = nil
end

-- Check for semihosting requests
local function check_trigger()
	-- Try to install if not already installed
	if not runtime.installed then
		install_semihost()
	end

	if not runtime.installed or not runtime.memory then
		return
	end

	-- Check if there's a valid RIFF structure at base_addr
	local bytes = read_memory_bytes(runtime.memory, config.base_addr, constants.RIFF_HEADER_SIZE)

	-- Check for RIFF signature
	if bytes[0] == 0x52 and bytes[1] == 0x49 and
	   bytes[2] == 0x46 and bytes[3] == 0x46 then
		-- Check for SEMI form type
		if bytes[8] == 0x53 and bytes[9] == 0x45 and
		   bytes[10] == 0x4D and bytes[11] == 0x49 then
			process_request()
		end
	end
end

-- Load configuration on machine start
local function load_config_from_disk()
	local loaded_config, err = persist.load_config(logger)
	if err then
		logger.warning('Config load issue: %s', err)
	end

	config = loaded_config
	logger.verbose('Loaded configuration from disk')
end

-- Save configuration when machine stops
function save_config()
	local success, err = persist.save_config(config, logger)
	if not success then
		logger.error('Failed to save config: %s', err or 'unknown error')
	else
		logger.verbose('Saved configuration to disk')
	end
end

-- Start plugin
function semihost.startplugin()
	-- Initialize with default config
	config = config_module.create_default()

	-- Create logger
	logger = logger_module.create(config)

	logger.verbose('Plugin loaded')

	-- Load saved configuration
	load_config_from_disk()

	-- Update logger with loaded config
	logger.set_logging_enabled(config.logging_enabled)

	-- Create sandbox manager
	sandbox = sandbox_module.create(config, logger)

	-- Create menu handler
	menu = menu_module.create(config, logger, update_config)

	-- Register menu
	emu.register_menu(
		function(index, event)
			return menu.handle(index, event)
		end,
		function()
			return menu.populate()
		end,
		_p('plugin-semihost', 'Semihosting')
	)

	-- Machine reset: install semihosting (plugin is enabled if we're running)
	reset_subscription = emu.add_machine_reset_notifier(function()
		install_semihost()
	end)

	-- Machine stop: cleanup and save config
	stop_subscription = emu.add_machine_stop_notifier(function()
		uninstall_semihost()
		save_config()
	end)

	-- Frame callback: check for semihosting requests
	frame_subscription = emu.add_machine_frame_notifier(function()
		check_trigger()
	end)

	logger.verbose('Plugin started - use plugin menu to configure')
end

return exports
