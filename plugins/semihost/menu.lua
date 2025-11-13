-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- Configuration menu for semihosting plugin

local menu = {}

function menu.create(semihost_state, save_callback)
	local self = {
		state = semihost_state,
		save_callback = save_callback,
		edit_sandbox_buffer = nil, -- nil when not editing, string when editing
	}

	-- Format address as hex string
	local function format_addr(addr)
		return string.format("0x%X", addr)
	end

	-- Parse hex address string
	local function parse_addr(str)
		local num = tonumber(str, 16)
		if not num then
			num = tonumber(str, 10)
		end
		return num
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
			format_addr(self.state.base_addr),
			'lr'
		})

		-- Sandbox directory (editable)
		local sandbox_display
		if self.edit_sandbox_buffer then
			sandbox_display = self.edit_sandbox_buffer .. '_'
		else
			sandbox_display = self.state.sandbox_dir
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
		local log_str = self.state.logging and _p('plugin-semihost', 'On') or _p('plugin-semihost', 'Off')
		table.insert(items, {
			_p('plugin-semihost', 'Verbose Logging'),
			log_str,
			self.state.logging and 'l' or 'r'
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
						addr_bits = cpu.spaces['program'].addr_width
					end
				end

				-- Calculate current number of zero bits
				local current_zeros = 0
				local test_addr = self.state.base_addr
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
				if current_zeros == addr_bits then
					self.state.base_addr = 0
				else
					-- Create mask of all 1s in the address space
					local all_ones = (1 << addr_bits) - 1
					-- Shift left by number of zeros to get 1s followed by 0s
					self.state.base_addr = (all_ones << current_zeros) & all_ones
				end

				emu.print_info(string.format('[SEMIHOST] Base address changed to %s', format_addr(self.state.base_addr)))
				if self.save_callback then
					self.save_callback()
				end
				return true
			end
		elseif index == ITEM_SANDBOX then
			-- Handle sandbox directory text editing
			if self.edit_sandbox_buffer then
				-- Already editing
				if event == 'select' then
					-- Save
					self.state.sandbox_dir = self.edit_sandbox_buffer
					self.edit_sandbox_buffer = nil
					emu.print_info(string.format('[SEMIHOST] Sandbox directory set to: %s', self.state.sandbox_dir))
					if self.save_callback then
						self.save_callback()
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
					self.edit_sandbox_buffer = self.state.sandbox_dir
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
				self.state.logging = not self.state.logging
				emu.print_info(string.format('[SEMIHOST] Verbose logging %s', self.state.logging and 'enabled' or 'disabled'))
				if self.save_callback then
					self.save_callback()
				end
				return true
			end
		end

		return false
	end

	return self
end

return menu
