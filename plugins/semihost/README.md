# RIFF-Based Semihosting Plugin for MAME

This plugin implements a memory-mapped semihosting device based on the RIFF container format. It provides a platform-independent mechanism for embedded systems to perform I/O operations through MAME during development and testing.

## Features

- **Architecture-agnostic**: Works with any word size (8-bit, 16-bit, 32-bit, 64-bit, etc.)
- **Memory-mapped**: No trap instructions or debugger required
- **Self-describing**: Uses RIFF format for explicit configuration
- **ARM compatible**: Implements ARM semihosting standard syscall numbers
- **Sandboxed**: Optional file system restrictions for security
- **Configurable**: Memory addresses and behavior customizable via menu

## Installation

The plugin is included with MAME in the `plugins/semihost` directory.

## Usage

### Enabling the Plugin

1. Start MAME with the plugin enabled:
   ```bash
   mame -plugin semihost <system>
   ```

2. Or enable it in `mame.ini`:
   ```ini
   plugin semihost
   ```

3. Access the configuration menu:
   - Press **Tab** to open the MAME menu
   - Navigate to **Plugin Options** → **Semihosting**

### Configuration Options

- **Semihosting**: Enable/disable the plugin
- **Base Address**: Memory-mapped region base address
  - Default for 8-bit systems: `0xFC00`
  - Default for others: `0xFFFFFC00`
- **Trigger Offset**: Offset from base for trigger writes (default: `0x1000`)
- **Sandbox Directory**: Root directory for file operations (default: `<homepath>/semihost`)
- **Verbose Logging**: Enable detailed syscall logging

### Memory Layout

The plugin monitors a memory region starting at the configured base address:

```
Base Address + 0x0000: RIFF header (12 bytes)
                       'RIFF' [size] 'SEMI'
Base Address + 0x000C: CNFG chunk (12 bytes)
                       Configuration (word size, endianness, etc.)
Base Address + 0x0018: CALL chunk (variable)
                       Syscall request
Base Address + 0x0018: RETN chunk (variable, written by plugin)
                       Syscall response (replaces CALL)
```

### RIFF Protocol

#### CNFG Chunk - Configuration

Declares the guest architecture parameters:

```
Offset  Size  Field       Description
------  ----  ----------  ---------------------------
+0x00   1     word_size   Bytes per word (1,2,4,8...)
+0x01   1     ptr_size    Bytes per pointer
+0x02   1     endianness  0=LE, 1=BE, 2=PDP
+0x03   1     reserved    Must be 0x00
```

#### CALL Chunk - Syscall Request

Requests a semihosting operation:

```
Offset  Size      Field     Description
------  -----     -------   ---------------------------
+0x00   1         opcode    ARM syscall number
+0x01   3         reserved  Must be 0x00
+0x04   ptr_size  arg_ptr   Pointer to argument array
```

#### RETN Chunk - Return Value

Plugin response with syscall result:

```
Offset      Size       Field   Description
------      -----      -----   ---------------------------
+0x00       word_size  result  Syscall return value
+word_size  4          errno   POSIX errno (0 = success)
```

## Supported Syscalls

The plugin implements all standard ARM semihosting syscalls:

### File I/O
- `SYS_OPEN (0x01)` - Open a file
- `SYS_CLOSE (0x02)` - Close a file
- `SYS_READ (0x06)` - Read from file
- `SYS_WRITE (0x05)` - Write to file
- `SYS_SEEK (0x0A)` - Seek in file
- `SYS_FLEN (0x0C)` - Get file length
- `SYS_REMOVE (0x0E)` - Delete file
- `SYS_RENAME (0x0F)` - Rename file
- `SYS_TMPNAM (0x0D)` - Get temporary filename

### Console I/O
- `SYS_WRITEC (0x03)` - Write character to debug channel
- `SYS_WRITE0 (0x04)` - Write null-terminated string
- `SYS_READC (0x07)` - Read a character

### System Information
- `SYS_CLOCK (0x10)` - Get clock ticks (centiseconds)
- `SYS_TIME (0x11)` - Get calendar time (seconds since epoch)
- `SYS_ELAPSED (0x30)` - Get elapsed time (64-bit microseconds)
- `SYS_TICKFREQ (0x31)` - Get tick frequency (1 MHz)
- `SYS_ERRNO (0x13)` - Get last errno value

### Utilities
- `SYS_ISERROR (0x08)` - Check error status
- `SYS_ISTTY (0x09)` - Check if file is a TTY
- `SYS_GET_CMDLINE (0x15)` - Get command line
- `SYS_HEAPINFO (0x16)` - Get heap information

### Control
- `SYS_EXIT (0x18)` - Exit application (pauses machine)
- `SYS_EXIT_EXTENDED (0x20)` - Exit with extended code
- `SYS_SYSTEM (0x12)` - Execute host command (blocked for security)

## Security

### File System Sandboxing

All file operations are restricted to the configured sandbox directory. The plugin:

- Blocks absolute paths
- Blocks `..` directory traversal
- Resolves all paths relative to sandbox root
- Creates sandbox directory automatically if it doesn't exist

Default sandbox location: `<mame_homepath>/semihost/`

### System Command Blocking

The `SYS_SYSTEM` syscall is blocked for security reasons and will return `EPERM` (permission denied).

## Example Guest Code (C)

```c
#include <stdint.h>

#define SEMIHOST_BASE 0xFC00

// Write RIFF header + CNFG + CALL
void semihost_write(const char *str) {
    volatile uint8_t *base = (volatile uint8_t *)SEMIHOST_BASE;

    // RIFF header
    base[0] = 'R'; base[1] = 'I'; base[2] = 'F'; base[3] = 'F';
    base[4] = 0x20; base[5] = 0; base[6] = 0; base[7] = 0; // size
    base[8] = 'S'; base[9] = 'E'; base[10] = 'M'; base[11] = 'I';

    // CNFG chunk
    base[12] = 'C'; base[13] = 'N'; base[14] = 'F'; base[15] = 'G';
    base[16] = 4; base[17] = 0; base[18] = 0; base[19] = 0; // size
    base[20] = 1;  // word_size = 1 byte
    base[21] = 2;  // ptr_size = 2 bytes
    base[22] = 0;  // little endian
    base[23] = 0;  // reserved

    // CALL chunk - SYS_WRITE0
    base[24] = 'C'; base[25] = 'A'; base[26] = 'L'; base[27] = 'L';
    base[28] = 6; base[29] = 0; base[30] = 0; base[31] = 0; // size
    base[32] = 0x04; // SYS_WRITE0
    base[33] = 0; base[34] = 0; base[35] = 0; // reserved
    base[36] = (uintptr_t)str & 0xFF;        // arg_ptr low
    base[37] = ((uintptr_t)str >> 8) & 0xFF; // arg_ptr high

    // Trigger (write to trigger offset)
    volatile uint8_t *trigger = (volatile uint8_t *)(SEMIHOST_BASE + 0x1000);
    *trigger = 1;

    // Wait for RETN (poll until CALL is replaced)
    while (base[24] == 'C' && base[25] == 'A' &&
           base[26] == 'L' && base[27] == 'L') {
        // Busy wait
    }
}
```

## Troubleshooting

### Plugin Not Appearing in Menu

1. Ensure the plugin is enabled in `mame.ini` or via `-plugin` command line
2. Check that all plugin files are present in `plugins/semihost/`
3. Verify `plugin.json` is properly formatted

### Semihosting Not Working

1. Check that "Semihosting" is enabled in the plugin menu
2. Verify the base address doesn't conflict with actual system memory
3. Enable "Verbose Logging" to see syscall activity
4. Check MAME console output for error messages

### File Operations Failing

1. Verify sandbox directory exists and is writable
2. Check file paths don't contain `..` or absolute paths
3. Ensure files exist in the sandbox directory
4. Check MAME console for specific error messages

## Technical Details

### Implementation

The plugin is implemented entirely in Lua using MAME's plugin API:

- **riff_parser.lua**: RIFF chunk parsing and encoding
- **syscalls.lua**: ARM semihosting syscall implementations
- **menu.lua**: Configuration UI
- **init.lua**: Main plugin logic and memory monitoring

### Trigger Mechanism

The plugin uses frame-based polling to detect semihosting requests. On each frame:

1. Check if RIFF signature exists at base address
2. If valid, parse CNFG and CALL chunks
3. Execute the requested syscall
4. Write RETN chunk back to memory

### Endianness Support

The plugin supports three endianness modes:
- **Little Endian (0)**: LSB first (x86, ARM Cortex-M)
- **Big Endian (1)**: MSB first (68k, SPARC, PowerPC)
- **PDP Endian (2)**: Middle-endian (PDP-11)

All multi-byte values respect the configured endianness.

## Specification

This implementation follows the **RIFF-Based Semihosting Device Specification v1.0-draft**.

For the complete specification, see: `~/git/semihost/semihost.md`

## License

BSD-3-Clause

## Authors

MAME Team

## Version History

- **1.0.0** (2025-11-12): Initial implementation
  - Full ARM semihosting syscall support
  - RIFF protocol parsing
  - File system sandboxing
  - Configuration menu
  - Multi-endianness support
