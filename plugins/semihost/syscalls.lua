-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- ARM Semihosting syscall implementations

local syscalls = {}

-- POSIX errno values
local ERRNO = {
	SUCCESS = 0,
	EPERM = 1,
	ENOENT = 2,
	ESRCH = 3,
	EINTR = 4,
	EIO = 5,
	ENXIO = 6,
	E2BIG = 7,
	ENOEXEC = 8,
	EBADF = 9,
	ECHILD = 10,
	EAGAIN = 11,
	ENOMEM = 12,
	EACCES = 13,
	EFAULT = 14,
	ENOTBLK = 15,
	EBUSY = 16,
	EEXIST = 17,
	EXDEV = 18,
	ENODEV = 19,
	ENOTDIR = 20,
	EISDIR = 21,
	EINVAL = 22,
	ENFILE = 23,
	EMFILE = 24,
	ENOTTY = 25,
	ETXTBSY = 26,
	EFBIG = 27,
	ENOSPC = 28,
	ESPIPE = 29,
	EROFS = 30,
	EMLINK = 31,
	EPIPE = 32,
}

-- ARM Semihosting syscall opcodes
syscalls.SYS_OPEN = 0x01
syscalls.SYS_CLOSE = 0x02
syscalls.SYS_WRITEC = 0x03
syscalls.SYS_WRITE0 = 0x04
syscalls.SYS_WRITE = 0x05
syscalls.SYS_READ = 0x06
syscalls.SYS_READC = 0x07
syscalls.SYS_ISERROR = 0x08
syscalls.SYS_ISTTY = 0x09
syscalls.SYS_SEEK = 0x0A
syscalls.SYS_FLEN = 0x0C
syscalls.SYS_TMPNAM = 0x0D
syscalls.SYS_REMOVE = 0x0E
syscalls.SYS_RENAME = 0x0F
syscalls.SYS_CLOCK = 0x10
syscalls.SYS_TIME = 0x11
syscalls.SYS_SYSTEM = 0x12
syscalls.SYS_ERRNO = 0x13
syscalls.SYS_GET_CMDLINE = 0x15
syscalls.SYS_HEAPINFO = 0x16
syscalls.SYS_EXIT = 0x18
syscalls.SYS_EXIT_EXTENDED = 0x20
syscalls.SYS_ELAPSED = 0x30
syscalls.SYS_TICKFREQ = 0x31

-- Open file mode flags
local OPEN_MODE = {
	[0] = "r",   -- SH_OPEN_R
	[1] = "rb",  -- SH_OPEN_RB
	[2] = "r+",  -- SH_OPEN_R_PLUS
	[3] = "r+b", -- SH_OPEN_R_PLUS_B
	[4] = "w",   -- SH_OPEN_W
	[5] = "wb",  -- SH_OPEN_WB
	[6] = "w+",  -- SH_OPEN_W_PLUS
	[7] = "w+b", -- SH_OPEN_W_PLUS_B
	[8] = "a",   -- SH_OPEN_A
	[9] = "ab",  -- SH_OPEN_AB
	[10] = "a+", -- SH_OPEN_A_PLUS
	[11] = "a+b", -- SH_OPEN_A_PLUS_B
}

-- Create syscall executor
function syscalls.create(config)
	local self = {
		config = config,
		open_files = {}, -- fd -> file handle mapping
		next_fd = 3, -- Start after stdin/stdout/stderr
		last_errno = 0,
		sandbox_dir = config.sandbox_dir or "",
		start_time = os.clock(),
	}

	-- Sanitize and validate file path within sandbox
	function self.sanitize_path(path)
		-- Remove any .. or absolute path components
		if path:match("^/") or path:match("%.%.") then
			return nil, ERRNO.EACCES
		end

		if self.sandbox_dir == "" then
			return path, ERRNO.SUCCESS
		end

		-- Build full path within sandbox
		local full_path = self.sandbox_dir .. "/" .. path
		return full_path, ERRNO.SUCCESS
	end

	-- Read arguments from guest memory
	function self.read_args(arg_ptr, count, memory, riff)
		local args = {}
		local word_size = self.config.word_size
		local endian = self.config.endianness

		for i = 0, count - 1 do
			local addr = arg_ptr + (i * word_size)
			local bytes = {}

			-- Read word_size bytes from memory
			for j = 0, word_size - 1 do
				bytes[j] = memory:read_u8(addr + j)
			end

			args[i] = riff.read_value(bytes, 0, word_size, endian)
		end

		return args
	end

	-- Read string from guest memory
	function self.read_string(addr, length, memory)
		local str = {}
		for i = 0, length - 1 do
			local byte = memory:read_u8(addr + i)
			if byte == 0 then
				break
			end
			table.insert(str, string.char(byte))
		end
		return table.concat(str)
	end

	-- Read null-terminated string from guest memory
	function self.read_string0(addr, memory, max_len)
		max_len = max_len or 4096
		local str = {}
		for i = 0, max_len - 1 do
			local byte = memory:read_u8(addr + i)
			if byte == 0 then
				break
			end
			table.insert(str, string.char(byte))
		end
		return table.concat(str)
	end

	-- Write buffer to guest memory
	function self.write_buffer(addr, buffer, memory)
		for i = 1, #buffer do
			memory:write_u8(addr + i - 1, string.byte(buffer, i))
		end
	end

	-- SYS_OPEN: Open a file
	function self.sys_open(args, memory, riff)
		local filename_ptr = args[0]
		local mode = args[1]
		local length = args[2]

		local filename = self.read_string(filename_ptr, length, memory)
		local sanitized, err = self.sanitize_path(filename)

		if not sanitized then
			self.last_errno = err
			return -1, err
		end

		local mode_str = OPEN_MODE[mode] or "r"
		local file, err_msg = io.open(sanitized, mode_str)

		if not file then
			emu.print_verbose(string.format("[SEMIHOST] Failed to open '%s': %s", sanitized, err_msg))
			self.last_errno = ERRNO.ENOENT
			return -1, ERRNO.ENOENT
		end

		local fd = self.next_fd
		self.next_fd = self.next_fd + 1
		self.open_files[fd] = file

		emu.print_verbose(string.format("[SEMIHOST] Opened '%s' as fd %d", sanitized, fd))
		return fd, ERRNO.SUCCESS
	end

	-- SYS_CLOSE: Close a file
	function self.sys_close(args, memory, riff)
		local fd = args[0]

		if fd < 3 then
			return 0, ERRNO.SUCCESS -- stdin/stdout/stderr are always open
		end

		local file = self.open_files[fd]
		if not file then
			self.last_errno = ERRNO.EBADF
			return -1, ERRNO.EBADF
		end

		file:close()
		self.open_files[fd] = nil
		emu.print_verbose(string.format("[SEMIHOST] Closed fd %d", fd))
		return 0, ERRNO.SUCCESS
	end

	-- SYS_WRITEC: Write a character to debug channel
	function self.sys_writec(args, memory, riff)
		local char_ptr = args[0]
		local ch = memory:read_u8(char_ptr)
		io.write(string.char(ch))
		io.flush()
		return 0, ERRNO.SUCCESS
	end

	-- SYS_WRITE0: Write null-terminated string to debug channel
	function self.sys_write0(args, memory, riff)
		local str_ptr = args[0]
		local str = self.read_string0(str_ptr, memory)
		io.write(str)
		io.flush()
		return 0, ERRNO.SUCCESS
	end

	-- SYS_WRITE: Write to file
	function self.sys_write(args, memory, riff)
		local fd = args[0]
		local buf_ptr = args[1]
		local count = args[2]

		-- Handle stdout/stderr
		if fd == 1 or fd == 2 then
			for i = 0, count - 1 do
				local byte = memory:read_u8(buf_ptr + i)
				io.write(string.char(byte))
			end
			io.flush()
			return count, ERRNO.SUCCESS
		end

		local file = self.open_files[fd]
		if not file then
			self.last_errno = ERRNO.EBADF
			return -1, ERRNO.EBADF
		end

		-- Read buffer from guest memory
		local buffer = {}
		for i = 0, count - 1 do
			table.insert(buffer, string.char(memory:read_u8(buf_ptr + i)))
		end

		local data = table.concat(buffer)
		local success, err_msg = file:write(data)

		if not success then
			emu.print_error(string.format("[SEMIHOST] Write failed: %s", err_msg))
			self.last_errno = ERRNO.EIO
			return -1, ERRNO.EIO
		end

		file:flush()
		return count, ERRNO.SUCCESS
	end

	-- SYS_READ: Read from file
	function self.sys_read(args, memory, riff)
		local fd = args[0]
		local buf_ptr = args[1]
		local count = args[2]

		-- Handle stdin
		if fd == 0 then
			local data = io.read(count)
			if not data then
				return 0, ERRNO.SUCCESS
			end
			self.write_buffer(buf_ptr, data, memory)
			return #data, ERRNO.SUCCESS
		end

		local file = self.open_files[fd]
		if not file then
			self.last_errno = ERRNO.EBADF
			return -1, ERRNO.EBADF
		end

		local data = file:read(count)
		if not data then
			return 0, ERRNO.SUCCESS
		end

		self.write_buffer(buf_ptr, data, memory)
		return #data, ERRNO.SUCCESS
	end

	-- SYS_READC: Read a character
	function self.sys_readc(args, memory, riff)
		local ch = io.read(1)
		if not ch then
			return -1, ERRNO.SUCCESS
		end
		return string.byte(ch), ERRNO.SUCCESS
	end

	-- SYS_ISERROR: Check if status is an error
	function self.sys_iserror(args, memory, riff)
		local status = args[0]
		-- Non-zero is error in semihosting
		return (status ~= 0) and 1 or 0, ERRNO.SUCCESS
	end

	-- SYS_ISTTY: Check if fd is a TTY
	function self.sys_istty(args, memory, riff)
		local fd = args[0]
		-- stdin/stdout/stderr are TTYs, files are not
		return (fd >= 0 and fd <= 2) and 1 or 0, ERRNO.SUCCESS
	end

	-- SYS_SEEK: Seek in file
	function self.sys_seek(args, memory, riff)
		local fd = args[0]
		local pos = args[1]

		local file = self.open_files[fd]
		if not file then
			self.last_errno = ERRNO.EBADF
			return -1, ERRNO.EBADF
		end

		local success, err_msg = file:seek("set", pos)
		if not success then
			self.last_errno = ERRNO.EIO
			return -1, ERRNO.EIO
		end

		return 0, ERRNO.SUCCESS
	end

	-- SYS_FLEN: Get file length
	function self.sys_flen(args, memory, riff)
		local fd = args[0]

		local file = self.open_files[fd]
		if not file then
			self.last_errno = ERRNO.EBADF
			return -1, ERRNO.EBADF
		end

		local current = file:seek()
		local size = file:seek("end")
		file:seek("set", current)

		return size, ERRNO.SUCCESS
	end

	-- SYS_TMPNAM: Get temporary filename
	function self.sys_tmpnam(args, memory, riff)
		local buf_ptr = args[0]
		local id = args[1]
		local length = args[2]

		local tmpname = string.format("tmp%05d.tmp", id)
		if #tmpname > length then
			self.last_errno = ERRNO.ERANGE
			return -1, ERRNO.ERANGE
		end

		self.write_buffer(buf_ptr, tmpname, memory)
		return 0, ERRNO.SUCCESS
	end

	-- SYS_REMOVE: Delete file
	function self.sys_remove(args, memory, riff)
		local filename_ptr = args[0]
		local length = args[1]

		local filename = self.read_string(filename_ptr, length, memory)
		local sanitized, err = self.sanitize_path(filename)

		if not sanitized then
			self.last_errno = err
			return -1, err
		end

		local success, err_msg = os.remove(sanitized)
		if not success then
			emu.print_verbose(string.format("[SEMIHOST] Failed to remove '%s': %s", sanitized, err_msg))
			self.last_errno = ERRNO.ENOENT
			return -1, ERRNO.ENOENT
		end

		return 0, ERRNO.SUCCESS
	end

	-- SYS_RENAME: Rename file
	function self.sys_rename(args, memory, riff)
		local old_ptr = args[0]
		local old_len = args[1]
		local new_ptr = args[2]
		local new_len = args[3]

		local old_name = self.read_string(old_ptr, old_len, memory)
		local new_name = self.read_string(new_ptr, new_len, memory)

		local old_sanitized, err1 = self.sanitize_path(old_name)
		local new_sanitized, err2 = self.sanitize_path(new_name)

		if not old_sanitized then
			self.last_errno = err1
			return -1, err1
		end

		if not new_sanitized then
			self.last_errno = err2
			return -1, err2
		end

		local success, err_msg = os.rename(old_sanitized, new_sanitized)
		if not success then
			emu.print_verbose(string.format("[SEMIHOST] Failed to rename '%s' to '%s': %s", old_sanitized, new_sanitized, err_msg))
			self.last_errno = ERRNO.EIO
			return -1, ERRNO.EIO
		end

		return 0, ERRNO.SUCCESS
	end

	-- SYS_CLOCK: Get clock ticks (centiseconds)
	function self.sys_clock(args, memory, riff)
		local elapsed = os.clock() - self.start_time
		return math.floor(elapsed * 100), ERRNO.SUCCESS
	end

	-- SYS_TIME: Get calendar time (seconds since epoch)
	function self.sys_time(args, memory, riff)
		return os.time(), ERRNO.SUCCESS
	end

	-- SYS_SYSTEM: Execute host command
	function self.sys_system(args, memory, riff)
		local cmd_ptr = args[0]
		local length = args[1]

		local cmd = self.read_string(cmd_ptr, length, memory)
		emu.print_warning(string.format("[SEMIHOST] SYS_SYSTEM blocked for security: '%s'", cmd))

		-- For security, don't execute host commands
		self.last_errno = ERRNO.EPERM
		return -1, ERRNO.EPERM
	end

	-- SYS_ERRNO: Get last errno
	function self.sys_errno(args, memory, riff)
		return self.last_errno, ERRNO.SUCCESS
	end

	-- SYS_GET_CMDLINE: Get command line
	function self.sys_get_cmdline(args, memory, riff)
		local buf_ptr = args[0]
		local length = args[1]

		-- Return empty command line
		if length > 0 then
			memory:write_u8(buf_ptr, 0)
		end

		return 0, ERRNO.SUCCESS
	end

	-- SYS_HEAPINFO: Get heap information
	function self.sys_heapinfo(args, memory, riff)
		local block_ptr = args[0]

		-- Write dummy heap info (4 words: heap_base, heap_limit, stack_base, stack_limit)
		-- Guest needs to provide its own heap info
		return 0, ERRNO.SUCCESS
	end

	-- SYS_EXIT: Exit application
	function self.sys_exit(args, memory, riff)
		local status = args[0]
		emu.print_info(string.format("[SEMIHOST] SYS_EXIT called with status %d", status))
		-- In MAME, we can't really exit, but we can pause
		manager.machine:pause()
		return 0, ERRNO.SUCCESS
	end

	-- SYS_EXIT_EXTENDED: Exit with extended code
	function self.sys_exit_extended(args, memory, riff)
		local exception = args[0]
		local subcode = args[1]
		emu.print_info(string.format("[SEMIHOST] SYS_EXIT_EXTENDED called: exception=%d, subcode=%d", exception, subcode))
		manager.machine:pause()
		return 0, ERRNO.SUCCESS
	end

	-- SYS_ELAPSED: Get elapsed time (64-bit ticks)
	function self.sys_elapsed(args, memory, riff)
		local elapsed = os.clock() - self.start_time
		local ticks = math.floor(elapsed * 1000000) -- microseconds
		return ticks, ERRNO.SUCCESS
	end

	-- SYS_TICKFREQ: Get tick frequency
	function self.sys_tickfreq(args, memory, riff)
		return 1000000, ERRNO.SUCCESS -- 1 MHz (microseconds)
	end

	-- Dispatch table
	local dispatch = {
		[syscalls.SYS_OPEN] = self.sys_open,
		[syscalls.SYS_CLOSE] = self.sys_close,
		[syscalls.SYS_WRITEC] = self.sys_writec,
		[syscalls.SYS_WRITE0] = self.sys_write0,
		[syscalls.SYS_WRITE] = self.sys_write,
		[syscalls.SYS_READ] = self.sys_read,
		[syscalls.SYS_READC] = self.sys_readc,
		[syscalls.SYS_ISERROR] = self.sys_iserror,
		[syscalls.SYS_ISTTY] = self.sys_istty,
		[syscalls.SYS_SEEK] = self.sys_seek,
		[syscalls.SYS_FLEN] = self.sys_flen,
		[syscalls.SYS_TMPNAM] = self.sys_tmpnam,
		[syscalls.SYS_REMOVE] = self.sys_remove,
		[syscalls.SYS_RENAME] = self.sys_rename,
		[syscalls.SYS_CLOCK] = self.sys_clock,
		[syscalls.SYS_TIME] = self.sys_time,
		[syscalls.SYS_SYSTEM] = self.sys_system,
		[syscalls.SYS_ERRNO] = self.sys_errno,
		[syscalls.SYS_GET_CMDLINE] = self.sys_get_cmdline,
		[syscalls.SYS_HEAPINFO] = self.sys_heapinfo,
		[syscalls.SYS_EXIT] = self.sys_exit,
		[syscalls.SYS_EXIT_EXTENDED] = self.sys_exit_extended,
		[syscalls.SYS_ELAPSED] = self.sys_elapsed,
		[syscalls.SYS_TICKFREQ] = self.sys_tickfreq,
	}

	-- Execute a syscall
	function self.execute(opcode, arg_ptr, memory, riff)
		local handler = dispatch[opcode]
		if not handler then
			emu.print_error(string.format("[SEMIHOST] Unknown syscall: 0x%02X", opcode))
			return -1, ERRNO.EINVAL
		end

		-- Determine argument count based on opcode
		local arg_count = 4 -- Default max args

		local args = self.read_args(arg_ptr, arg_count, memory, riff)
		return handler(args, memory, riff)
	end

	-- Cleanup open files
	function self.cleanup()
		for fd, file in pairs(self.open_files) do
			file:close()
		end
		self.open_files = {}
	end

	return self
end

return syscalls
