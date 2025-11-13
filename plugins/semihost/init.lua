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

local riff = require('semihost/riff_parser')
local syscalls_module = require('semihost/syscalls')
local menu_module = require('semihost/menu')
local persist = require('semihost/persist')

local reset_subscription, stop_subscription, frame_subscription

-- Plugin state
local state = {
	base_addr = 0xFFFFFC00,
	sandbox_dir = '',
	logging = false,
	installed = false,
	config = nil, -- Current CNFG chunk data
	syscalls = nil, -- Syscall executor
	memory = nil, -- Current address space
	menu = nil,
}

-- Determine default base address based on CPU address width
local function get_default_base_addr()
	if not manager.machine.devices[':maincpu'] then
		return 0xFFFFFC00
	end

	local cpu = manager.machine.devices[':maincpu']
	if not cpu.spaces or not cpu.spaces['program'] then
		return 0xFFFFFC00
	end

	local addr_space = cpu.spaces['program']
	local addr_bits = addr_space.addr_width

	-- For 8-bit and 16-bit systems, use lower address
	if addr_bits <= 16 then
		return 0xFC00
	else
		return 0xFFFFFC00
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

-- Process semihosting request
local function process_request()
	if not state.installed or not state.memory then
		return
	end

	-- Read RIFF structure from base address
	local riff_size = 1024 -- Read enough for header + chunks
	local bytes = read_memory_bytes(state.memory, state.base_addr, riff_size)

	-- Parse RIFF chunks
	local chunks, err = riff.parse_chunks(bytes)
	if not chunks then
		emu.print_error(string.format('[SEMIHOST] Failed to parse RIFF: %s', err))
		return
	end

	-- Find CNFG chunk and update configuration
	local cnfg_chunk = riff.find_chunk(chunks, 'CNFG')
	if cnfg_chunk then
		local cnfg, err = riff.parse_cnfg(bytes, cnfg_chunk)
		if cnfg then
			state.config = cnfg

			if state.logging then
				emu.print_verbose(string.format('[SEMIHOST] Configuration: word_size=%d, ptr_size=%d, endianness=%d',
					cnfg.word_size, cnfg.ptr_size, cnfg.endianness))
			end

			-- Recreate syscall executor with new config
			local syscall_config = {
				word_size = cnfg.word_size,
				ptr_size = cnfg.ptr_size,
				endianness = cnfg.endianness,
				sandbox_dir = state.sandbox_dir,
			}
			state.syscalls = syscalls_module.create(syscall_config)
		else
			emu.print_error(string.format('[SEMIHOST] Invalid CNFG chunk: %s', err))
			return
		end
	end

	-- Find CALL chunk
	local call_chunk = riff.find_chunk(chunks, 'CALL')
	if not call_chunk or not state.config or not state.syscalls then
		return
	end

	-- Parse CALL chunk
	local call, err = riff.parse_call(bytes, call_chunk, state.config.ptr_size, state.config.endianness)
	if not call then
		emu.print_error(string.format('[SEMIHOST] Invalid CALL chunk: %s', err))
		return
	end

	if state.logging then
		emu.print_verbose(string.format('[SEMIHOST] Syscall 0x%02X, arg_ptr=0x%X', call.opcode, call.arg_ptr))
	end

	-- Execute syscall
	local result, errno = state.syscalls.execute(call.opcode, call.arg_ptr, state.memory, riff)

	if state.logging then
		emu.print_verbose(string.format('[SEMIHOST] Result: %d, errno: %d', result, errno))
	end

	-- Create RETN chunk
	local retn_bytes = riff.create_retn(result, errno, state.config.word_size, state.config.endianness)

	-- Write RETN chunk back to memory (replacing CALL chunk)
	local retn_addr = state.base_addr + call_chunk.offset - 8 -- Subtract header size
	write_memory_bytes(state.memory, retn_addr, retn_bytes)
end

-- Install memory monitoring
local function install_semihost()
	if state.installed then
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
		emu.print_warning('[SEMIHOST] Main CPU has no program space')
		return
	end

	state.memory = cpu.spaces['program']

	-- Update default base address if not configured
	if state.base_addr == 0xFFFFFC00 then
		state.base_addr = get_default_base_addr()
	end

	emu.print_info(string.format('[SEMIHOST] Installing at base=0x%X', state.base_addr))

	-- Install write tap on trigger address
	-- Note: MAME Lua doesn't have direct write tap support, so we'll use periodic checking
	state.installed = true

	-- Set up sandbox directory
	if state.sandbox_dir == '' then
		local homepath = manager.options.entries.homepath:value():match('([^;]+)')
		state.sandbox_dir = homepath .. '/semihost'

		-- Try to create sandbox directory
		local lfs = require('lfs')
		local attr = lfs.attributes(state.sandbox_dir)
		if not attr then
			local success, err = lfs.mkdir(state.sandbox_dir)
			if success then
				emu.print_info(string.format('[SEMIHOST] Created sandbox directory: %s', state.sandbox_dir))
			else
				emu.print_warning(string.format('[SEMIHOST] Could not create sandbox directory: %s', err or 'unknown error'))
			end
		end
	end

	emu.print_info('[SEMIHOST] Semihosting device ready')
end

-- Uninstall memory monitoring
local function uninstall_semihost()
	if not state.installed then
		return
	end

	emu.print_info('[SEMIHOST] Uninstalling semihosting device')

	-- Clean up open files
	if state.syscalls then
		state.syscalls.cleanup()
	end

	state.installed = false
	state.memory = nil
	state.config = nil
	state.syscalls = nil
end

-- Check for semihosting requests
local function check_trigger()
	-- Try to install if not already installed
	if not state.installed then
		install_semihost()
	end

	if not state.installed or not state.memory then
		return
	end

	-- Check if there's a valid RIFF structure at base_addr
	local bytes = read_memory_bytes(state.memory, state.base_addr, 12)

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
local function load_config()
	local config = persist:load_config()

	-- Apply loaded config to state
	if config.base_addr then
		state.base_addr = config.base_addr
	end
	state.sandbox_dir = config.sandbox_dir
	state.logging = config.logging

	if state.logging then
		emu.print_verbose('[SEMIHOST] Loaded configuration from disk')
	end
end

-- Save configuration when machine stops
local function save_config()
	if persist:save_config(state) then
		if state.logging then
			emu.print_verbose('[SEMIHOST] Saved configuration to disk')
		end
	end
end

-- Start plugin
function semihost.startplugin()
	emu.print_info('[SEMIHOST] Plugin loaded')

	-- Load saved configuration
	load_config()

	-- Create menu handler with save callback
	state.menu = menu_module.create(state, save_config)

	-- Register menu
	emu.register_menu(
		function(index, event)
			return state.menu.handle(index, event)
		end,
		function()
			return state.menu.populate()
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

	emu.print_info('[SEMIHOST] Plugin started - use plugin menu to configure')
end

return exports
