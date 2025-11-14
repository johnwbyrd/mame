-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- Configuration menu for semihosting plugin

local constants = require('semihost/constants')
local config_module = require('semihost/config')
local menu = {}

-- Create menu handler
-- config: current configuration object (immutable)
-- logger: logger instance
-- on_config_change: callback(new_config) called when configuration should be updated
function menu.create(config, logger, on_config_change)
	local self = {
		config = config,
		logger = logger,
		on_config_change = on_config_change,
		edit_sandbox_buffer = nil, -- nil when not editing, string when editing
	}

	-- Format address as hex string
	local function format_addr(addr)
		return string.format("0x%X", addr)
	end

	-- Update configuration (called by parent)
	function self.update_config(new_config)
		self.config = new_config
	end

	-- Populate menu items
	function self.populate()
		local items = {}

		-- Header
		table.insert(items, {_p('plugin-semihost', 'Semihosting Configuration'), '', 'off'})
		table.insert(items, {'---', '', ''})

		-- Base address - show left/right arrows
		table.insert(items, {
			_p('plugin-semihost', 'Base Address'),
			format_addr(self.config.base_addr),
			'lr'
		})

		-- Sandbox directory (editable)
		local sandbox_display
		if self.edit_sandbox_buffer then
			sandbox_display = self.edit_sandbox_buffer .. '_'
		else
			sandbox_display = self.config.sandbox_dir
			if sandbox_display == '' then
				sandbox_display = _p('plugin-semihost', '<auto>')
			end
		end
		table.insert(items, {
			_p('plugin-semihost', 'Sandbox Directory'),
			sandbox_display,
			''
		})

		-- Logging
		local log_str = self.config.logging_enabled and _p('plugin-semihost', 'On') or _p('plugin-semihost', 'Off')
		table.insert(items, {
			_p('plugin-semihost', 'Verbose Logging'),
			log_str,
			self.config.logging_enabled and 'l' or 'r'
		})

		return items, nil, 'lrrepeat' .. (self.edit_sandbox_buffer and ' ignorepause' or '')
	end

	-- Handle menu events
	function self.handle(index, event)
		-- Character validation for text input
		local function inputchar()
			local ch = tonumber(event)
			if not ch then
				return nil
			elseif (ch >= 0x100) or ((ch & 0x7f) >= 0x20) or (ch == 0x08) then
				return utf8.char(ch)
			else
				return nil
			end
		end

		-- Item indices (accounting for header and separator)
		local ITEM_BASE_ADDR = 3
		local ITEM_SANDBOX = 4
		local ITEM_LOGGING = 5

		if index == ITEM_BASE_ADDR then
			if event == 'left' or event == 'right' or event == 'select' then
				-- Determine address space width
				local addr_bits = 32 -- default

				if manager and manager.machine and manager.machine.devices and manager.machine.devices[':maincpu'] then
					local cpu = manager.machine.devices[':maincpu']
					if cpu and cpu.spaces and cpu.spaces['program'] then
						local width = cpu.spaces['program'].addr_width
						if width then
							addr_bits = width
						end
					end
				end

				-- Calculate current number of zero bits
				local current_zeros = 0
				local test_addr = self.config.base_addr
				while current_zeros < addr_bits and (test_addr & 1) == 0 do
					current_zeros = current_zeros + 1
					test_addr = test_addr >> 1
				end

				-- Adjust based on direction
				if event == 'left' then
					current_zeros = current_zeros - 1
					if current_zeros < 0 then
						current_zeros = addr_bits
					end
				else -- right or select
					current_zeros = current_zeros + 1
					if current_zeros > addr_bits then
						current_zeros = 0
					end
				end

				-- Build address: all 1s followed by zeros
				local new_base_addr
				if current_zeros == addr_bits then
					new_base_addr = 0
				else
					-- Create mask of all 1s in the address space
					local all_ones = (1 << addr_bits) - 1
					-- Shift left by number of zeros to get 1s followed by 0s
					new_base_addr = (all_ones << current_zeros) & all_ones
				end

				self.logger.info('Base address changed to %s', format_addr(new_base_addr))

				-- Notify parent of configuration change
				if self.on_config_change then
					self.on_config_change({base_addr = new_base_addr})
				end
				return true
			end
		elseif index == ITEM_SANDBOX then
			-- Handle sandbox directory text editing
			if self.edit_sandbox_buffer then
				-- Already editing
				if event == 'select' then
					-- Save
					local new_sandbox_dir = self.edit_sandbox_buffer
					self.edit_sandbox_buffer = nil
					self.logger.info('Sandbox directory set to: %s', new_sandbox_dir)

					-- Notify parent of configuration change
					if self.on_config_change then
						self.on_config_change({sandbox_dir = new_sandbox_dir})
					end
					return true
				elseif event == 'back' then
					-- Swallow back key to prevent menu exit while editing
					return true
				elseif event == 'cancel' then
					-- Cancel editing
					self.edit_sandbox_buffer = nil
					return true
				else
					local char = inputchar()
					if char == '\b' then
						-- Backspace (UTF-8 safe)
						self.edit_sandbox_buffer = self.edit_sandbox_buffer:gsub('[%z\1-\127\192-\255][\128-\191]*$', '')
						return true
					elseif char then
						-- Add character
						self.edit_sandbox_buffer = self.edit_sandbox_buffer .. char
						return true
					end
				end
			else
				-- Not editing yet
				if event == 'select' then
					-- Start editing with current value
					self.edit_sandbox_buffer = self.config.sandbox_dir
					return true
				else
					local char = inputchar()
					if char == '\b' then
						-- Start editing with empty string
						self.edit_sandbox_buffer = ''
						return true
					elseif char then
						-- Start editing with this character
						self.edit_sandbox_buffer = char
						return true
					end
				end
			end
		elseif index == ITEM_LOGGING then
			if event == 'left' or event == 'right' then
				local new_logging = not self.config.logging_enabled

				-- Update our config immediately so populate() sees the new value
				local new_config, err = config_module.update(self.config, {logging_enabled = new_logging})
				if new_config then
					self.config = new_config
				end

				-- Notify parent of configuration change to update logger and save
				if self.on_config_change then
					self.on_config_change({logging_enabled = new_logging})
				end

				self.logger.verbose('Verbose logging %s', new_logging and 'enabled' or 'disabled')
				return true
			end
		end

		return false
	end

	return self
end

return menu
