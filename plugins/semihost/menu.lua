-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- Configuration menu for semihosting plugin

local menu = {}

function menu.create(semihost_state)
	local self = {
		state = semihost_state,
		editing = nil, -- {field, value}
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
		table.insert(items, {_p('plugin-semihost', 'RIFF Semihosting Configuration'), '', 'off'})
		table.insert(items, {'---', '', ''})

		-- Enable/Disable
		local enabled_str = self.state.enabled and _p('plugin-semihost', 'Enabled') or _p('plugin-semihost', 'Disabled')
		table.insert(items, {
			_p('plugin-semihost', 'Semihosting'),
			enabled_str,
			self.state.enabled and 'l' or 'r'
		})

		-- Base address
		table.insert(items, {
			_p('plugin-semihost', 'Base Address'),
			format_addr(self.state.base_addr),
			self.editing and self.editing.field == 'base_addr' and 'lr' or ''
		})

		-- Trigger offset
		table.insert(items, {
			_p('plugin-semihost', 'Trigger Offset'),
			format_addr(self.state.trigger_offset),
			self.editing and self.editing.field == 'trigger_offset' and 'lr' or ''
		})

		-- Sandbox directory
		local sandbox_display = self.state.sandbox_dir
		if sandbox_display == '' then
			sandbox_display = _p('plugin-semihost', '<none - unrestricted>')
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

		table.insert(items, {'---', '', ''})

		-- Status info
		if self.state.enabled and self.state.installed then
			table.insert(items, {
				_p('plugin-semihost', 'Status'),
				_p('plugin-semihost', 'Active'),
				'off'
			})

			if self.state.config then
				local config = self.state.config
				table.insert(items, {
					_p('plugin-semihost', 'Word Size'),
					string.format('%d bytes', config.word_size),
					'off'
				})
				table.insert(items, {
					_p('plugin-semihost', 'Pointer Size'),
					string.format('%d bytes', config.ptr_size),
					'off'
				})

				local endian_names = {[0] = 'Little', [1] = 'Big', [2] = 'PDP'}
				table.insert(items, {
					_p('plugin-semihost', 'Endianness'),
					endian_names[config.endianness] or 'Unknown',
					'off'
				})
			end
		elseif self.state.enabled then
			table.insert(items, {
				_p('plugin-semihost', 'Status'),
				_p('plugin-semihost', 'Waiting for machine start'),
				'off'
			})
		else
			table.insert(items, {
				_p('plugin-semihost', 'Status'),
				_p('plugin-semihost', 'Disabled'),
				'off'
			})
		end

		return items
	end

	-- Handle menu events
	function self.handle(index, event)
		-- Item indices (accounting for header and separator)
		local ITEM_ENABLE = 3
		local ITEM_BASE_ADDR = 4
		local ITEM_TRIGGER_OFFSET = 5
		local ITEM_SANDBOX = 6
		local ITEM_LOGGING = 7

		if index == ITEM_ENABLE then
			if event == 'left' or event == 'right' then
				self.state.enabled = not self.state.enabled
				if self.state.enabled then
					emu.print_info('[SEMIHOST] Enabled - will activate on next machine start')
				else
					emu.print_info('[SEMIHOST] Disabled')
				end
				return true
			end
		elseif index == ITEM_BASE_ADDR then
			if event == 'select' then
				-- TODO: Text input for address
				-- For now, cycle through some common addresses
				local addrs_8bit = {0xFC00, 0xFD00, 0xFE00, 0xFF00}
				local addrs_other = {0xFFFFFC00, 0xFFFFF000, 0xF0000000}

				local addr_space = manager.machine.devices[':maincpu'].spaces['program']
				local addr_bits = addr_space.addr_width

				local addrs = (addr_bits <= 16) and addrs_8bit or addrs_other
				local found = false

				for i, addr in ipairs(addrs) do
					if self.state.base_addr == addr then
						self.state.base_addr = addrs[(i % #addrs) + 1]
						found = true
						break
					end
				end

				if not found then
					self.state.base_addr = addrs[1]
				end

				emu.print_info(string.format('[SEMIHOST] Base address changed to %s', format_addr(self.state.base_addr)))
				return true
			end
		elseif index == ITEM_TRIGGER_OFFSET then
			if event == 'select' then
				-- Cycle through common trigger offsets
				local offsets = {0x1000, 0x2000, 0x0100, 0x0200}
				local found = false

				for i, offset in ipairs(offsets) do
					if self.state.trigger_offset == offset then
						self.state.trigger_offset = offsets[(i % #offsets) + 1]
						found = true
						break
					end
				end

				if not found then
					self.state.trigger_offset = offsets[1]
				end

				emu.print_info(string.format('[SEMIHOST] Trigger offset changed to %s', format_addr(self.state.trigger_offset)))
				return true
			end
		elseif index == ITEM_LOGGING then
			if event == 'left' or event == 'right' then
				self.state.logging = not self.state.logging
				emu.print_info(string.format('[SEMIHOST] Verbose logging %s', self.state.logging and 'enabled' or 'disabled'))
				return true
			end
		end

		return false
	end

	return self
end

return menu
