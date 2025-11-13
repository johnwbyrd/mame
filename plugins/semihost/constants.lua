-- license:BSD-3-Clause
-- copyright-holders:MAME Team
-- Constants and enumerations for semihosting plugin

local constants = {}

-- Default configuration values
constants.DEFAULT_BASE_ADDR = 0xFFFFFC00
constants.DEFAULT_SANDBOX_DIR = ''
constants.DEFAULT_LOGGING = false

-- RIFF protocol constants
constants.RIFF_BUFFER_SIZE = 1024
constants.RIFF_HEADER_SIZE = 12
constants.RIFF_CHUNK_HEADER_SIZE = 8

-- File descriptor constants
constants.FD_STDIN = 0
constants.FD_STDOUT = 1
constants.FD_STDERR = 2
constants.FD_FIRST_USER = 3  -- First available FD for user files

-- Maximum string read length
constants.MAX_STRING_LENGTH = 4096

-- POSIX errno values
constants.ERRNO = {
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
	ERANGE = 34,  -- Added for TMPNAM
}

-- ARM Semihosting syscall opcodes
constants.SYS_OPEN = 0x01
constants.SYS_CLOSE = 0x02
constants.SYS_WRITEC = 0x03
constants.SYS_WRITE0 = 0x04
constants.SYS_WRITE = 0x05
constants.SYS_READ = 0x06
constants.SYS_READC = 0x07
constants.SYS_ISERROR = 0x08
constants.SYS_ISTTY = 0x09
constants.SYS_SEEK = 0x0A
constants.SYS_FLEN = 0x0C
constants.SYS_TMPNAM = 0x0D
constants.SYS_REMOVE = 0x0E
constants.SYS_RENAME = 0x0F
constants.SYS_CLOCK = 0x10
constants.SYS_TIME = 0x11
constants.SYS_SYSTEM = 0x12
constants.SYS_ERRNO = 0x13
constants.SYS_GET_CMDLINE = 0x15
constants.SYS_HEAPINFO = 0x16
constants.SYS_EXIT = 0x18
constants.SYS_EXIT_EXTENDED = 0x20
constants.SYS_ELAPSED = 0x30
constants.SYS_TICKFREQ = 0x31

-- Open file mode flags (ARM semihosting mode to Lua file mode)
constants.OPEN_MODE = {
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

-- Common base addresses for different ARM systems
constants.COMMON_BASE_ADDRESSES = {
	0xFFFFFC00,  -- Default, common for many ARM systems
	0x20000000,  -- SRAM base on some Cortex-M
	0x00000000,  -- Bottom of address space
}

-- Log level enumeration
constants.LOG_LEVEL = {
	ERROR = 1,
	WARNING = 2,
	INFO = 3,
	VERBOSE = 4,
	DEBUG = 5,
}

return constants
