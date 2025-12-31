Zero Board Computer (ZBC)
==========================

.. contents:: :local:


1. Overview
-----------

1.1 Purpose
~~~~~~~~~~~

The ZBC (Zero Board Computer) system provides minimal, standardized test
environments for all emulatable CPU architectures in MAME. Each ZBC variant
consists of a CPU, RAM, text display, and semihosting interface, allowing
programs to be loaded and executed via ELF loader with host I/O support.

The zbcgen tool automates the discovery, generation, and maintenance of ZBC
definitions, serving as the canonical, comprehensive list of CPU support in
MAME with automated quality control.

1.2 Motivation
~~~~~~~~~~~~~~

MAME supports hundreds of CPU architectures across decades of computing history.
Manually maintaining test systems for each CPU is error-prone and incomplete.
The zbcgen system automates:

* **Discovery**: Enumerate all CPU devices from MAME's device registry
* **Generation**: Create minimal test systems for each CPU automatically
* **Validation**: Track which CPUs compile and validate successfully
* **Maintenance**: Update CPU status as MAME evolves

1.3 Design Philosophy
~~~~~~~~~~~~~~~~~~~~~

The ZBC system uses C++ templates to eliminate code duplication. A single
template class (``zbc_state``) is instantiated for each CPU type, with
compile-time customization of clock speed and video RAM placement.
CPU-specific initialization (reset vectors, exception tables) is handled
via template specialization.

The ``DEFINE_ZBC`` macro generates complete machine variants from a single
line, creating unique classes and registering them with MAME.


2. Architecture
---------------

2.1 System Components
~~~~~~~~~~~~~~~~~~~~~

The zbcgen system consists of:

**Source Files (Manual)**:
  * ``src/mame/zbc/zbc.cpp`` - Template classes, helper functions, CPU-specific specializations

**Generated Files (Automatic)**:
  * ``src/mame/zbc/zbcgen.hpp`` - All CPU header ``#include`` directives (alphabetical)
  * ``src/mame/zbc/zbcgen.ipp`` - All ``DEFINE_ZBC()`` macro invocations (included by zbc.cpp)

**Build Tools**:
  * ``scripts/build/zbcgen.py`` - Main generator script
  * ``src/mame/zbc/zbc_status.csv`` - Knowledge base tracking CPU status (version controlled)

**MAME Infrastructure**:
  * Enhanced ``-listcpu`` command - Outputs CPU metadata
  * Enhanced device type system - Tracks type constant names at runtime
  * ``src/mame/mame.lst`` - Driver registration list (updated by zbcgen.py)

2.2 ZBC Template System
~~~~~~~~~~~~~~~~~~~~~~~

Each ZBC system includes:

* **RAM**: Sized automatically based on CPU address space width
* **MC6847 VDG**: Text display (32x16 characters) at top of address space
* **Semihosting**: Memory-mapped semihost device (32-byte register interface)
* **ELF Loader**: Support for loading static ELF executables (``-elfload``)
* **CPU Init**: Architecture-specific boot code (reset vectors, etc.)
* **Timer**: Programmable timer interrupt via semihosting (see 2.3)

2.3 Timer Configuration (SYS_TIMER_CONFIG)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ZBC semihosting device provides a programmable timer interrupt that can
fire at any frequency from 1 Hz to 10 MHz. This replaces the older JP1 jumper
system with a more flexible software-controlled approach.

**Configuration via Semihosting**:

Timer interrupts are configured using the ``SYS_TIMER_CONFIG`` syscall
(opcode 0x32) through the RIFF semihosting protocol:

* **rate_hz = 0**: Disable timer (default state after reset)
* **rate_hz > 0**: Enable periodic timer at the specified frequency in Hz

**Common Timer Rates**:

* **50 Hz**: Retro/8-bit systems, PAL frame sync
* **60 Hz**: NTSC frame sync
* **100 Hz**: Embedded systems, older Linux (HZ=100)
* **1000 Hz**: Modern Linux, RTOS (HZ=1000)

**STATUS Register (0x19)**:

The semihosting STATUS register indicates pending interrupts:

* **Value 0**: No interrupt pending
* **Value 1**: Timer tick occurred
* **Value 2+**: Reserved for future interrupt sources

To acknowledge the interrupt and deassert the IRQ line, write 0 to STATUS.
The timer continues running; the next tick will set STATUS and assert IRQ again.

**Interrupt Handling**:

When a timer tick occurs:

1. STATUS register is set to 1
2. CPU IRQ0 line is asserted (ASSERT_LINE)
3. ISR reads STATUS to confirm timer interrupt
4. ISR handles the interrupt
5. ISR writes 0 to STATUS to acknowledge
6. IRQ line is deasserted (CLEAR_LINE)
7. ISR returns with appropriate instruction (RTI, RETI, etc.)

**Programming Considerations**:

Programs using timer interrupts must:

1. **Install interrupt handlers** at the appropriate vector addresses:

   - Z80 IRQ: Mode-dependent (IM 0/1/2)
   - 6502 IRQ vector: 0xFFFE-0xFFFF
   - ARM: Vector table at 0x00 or 0xFFFF0000
   - (other CPUs: consult CPU-specific documentation)

2. **Return from interrupt** using the appropriate instruction:

   - Z80 IRQ: RETI (0xED 0x4D)
   - 6502: RTI (0x40)
   - ARM: SUBS PC, LR, #4 or equivalent
   - (other CPUs: consult CPU-specific documentation)

3. **Acknowledge the interrupt** by writing 0 to STATUS register

**Error Returns**:

* **ZBC_ERR_OK (0)**: Timer configured successfully
* **ZBC_ERR_INVALID_ARG (-13)**: Rate too high (>10 MHz)

2.4 Dynamic Memory Layout Calculation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ZBC memory layout is **dynamically calculated** based on CPU address width
using these algorithms:

**Reserved Region Start**::

    reserved_start = 2^addr_bits - 2^(addr_bits/2)

This scales the reserved region proportionally with address space:

* 16-bit (64KB): ``2^16 - 2^8 = 0xFF00`` (256 bytes reserved)
* 24-bit (16MB): ``2^24 - 2^12 = 0xFFF000`` (4096 bytes reserved)
* 32-bit (4GB): ``2^32 - 2^16 = 0xFFFF0000`` (65536 bytes reserved)
* 64-bit (16EB): ``2^64 - 2^32 = 0xFFFFFFFF00000000`` (4GB reserved)

**Video RAM Address**::

    vram_addr = reserved_start - 512

(unless overridden by VRAM_ADDR template parameter)

**Semihost Device Address**::

    semihost_addr = vram_addr - 32

**Available RAM**::

    ram_start = 0x0000
    ram_end = semihost_addr - 1
    available_ram = semihost_addr

**Example: 16-bit CPU (Z80, 6502, etc.)**::

    Address Space: 16-bit (64KB total)

    reserved_start = 0xFF00
    vram_addr      = 0xFF00 - 512    = 0xFD00
    semihost_addr  = 0xFD00 - 32     = 0xFCE0
    available_ram  = 0xFCE0           = 64,736 bytes

    Memory Map:
    0x0000-0xFCDF   Available RAM (64,736 bytes)
    0xFCE0-0xFCFF   Semihost device (32 bytes)
    0xFD00-0xFEFF   Video RAM (512 bytes)
    0xFF00-0xFFFF   Reserved region (256 bytes, includes 6502 vectors)

**Example: 32-bit CPU (i386, 68000, ARM, etc.)**::

    Address Space: 32-bit (4GB total)

    reserved_start = 0xFFFF0000
    vram_addr      = 0xFFFF0000 - 512  = 0xFFFEFE00
    semihost_addr  = 0xFFFEFE00 - 32   = 0xFFFEFDE0
    available_ram  = 0xFFFEFDE0         = 4,294,705,632 bytes (~4GB)

    Memory Map:
    0x00000000-0xFFFEFDDF   Available RAM (~4GB)
    0xFFFEFDE0-0xFFFEFDFF   Semihost device (32 bytes)
    0xFFFEFE00-0xFFFEFFFF   Video RAM (512 bytes)
    0xFFFF0000-0xFFFFFFFF   Reserved region (65,536 bytes)

The layout scales automatically across all address space sizes, ensuring
consistent peripheral placement while maximizing available RAM.

2.5 DEFINE_ZBC Macro
~~~~~~~~~~~~~~~~~~~~

The ``DEFINE_ZBC`` macro in ``zbc.cpp`` generates a complete ZBC variant::

    DEFINE_ZBC(m6502_device, M6502, m6502, "MOS Technology 6502")

This expands to:

1. A derived class (``zbc_m6502_state``) inheriting from ``zbc_state<m6502_device>``
2. ROM definition (empty for ZBC systems)
3. MAME machine registration (``COMP`` macro)

Parameters:
  * ``cpu_class``: CPU device class for template instantiation (e.g., ``m6502_device``)
  * ``cpu_type``: MAME device type macro for machine_config (e.g., ``M6502``)
  * ``short_name``: Machine name suffix, creates ``zbc<short_name>`` (e.g., ``m6502`` → ``zbcm6502``)
  * ``display_name``: Human-readable name for UI (e.g., ``"MOS Technology 6502"``)
  * ``...``: Optional template parameter overrides (cpu_speed, vram_addr)

Optional parameters allow customization::

    DEFINE_ZBC(pdp1_device, PDP1, pdp1_cpu, "DEC PDP-1 Central Processor", 200000)
    // Sets CPU_SPEED=200kHz


3. Knowledge Base (zbc_status.csv)
----------------------------------

3.1 Schema
~~~~~~~~~~

The CSV file serves as the single source of truth for CPU status::

    shortname,type_constant,class_name,fullname,status,header_file,notes

Fields:
  * ``shortname``: Short identifier (e.g., ``"m6502"``)
  * ``type_constant``: Device type constant (e.g., ``"M6502"``)
  * ``class_name``: C++ device class name (e.g., ``"m6502_device"``)
  * ``fullname``: Human-readable name (e.g., ``"MOS Technology 6502"``)
  * ``status``: Current state (see 3.2)
  * ``header_file``: Include path (e.g., ``"cpu/m6502/m6502.h"``)
  * ``notes``: Freeform text (error messages, reasons for disabled status)

3.2 Status Values
~~~~~~~~~~~~~~~~~

* ``working``: CPU compiles, validates, and runs correctly
* ``broken_compile``: Compilation fails (syntax errors, missing dependencies)
* ``broken_validate``: Compiles but fails ``mame -validate`` checks
* ``broken_header``: Header file not found or doesn't declare device type
* ``disabled``: Manually disabled (requires special configuration, conflicting macros, driver name exceeds MAME limits)
* ``not_cpu``: Device is not a CPU (DMA controller, graphics interface, etc.)
* ``needs_internal_rom``: Requires internal ROM/RAM regions not provided by ZBC (PICs, AVR, 8051, etc.)
* ``needs_dependent_device``: Requires additional dependent devices (PlayStation 2 VU units, Emotion Engine, etc.)
* ``unknown``: Not yet tested (initial state)


4. zbcgen.py Tool
-----------------

4.1 Command-Line Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``zbcgen.py`` script supports multiple operation modes:

**Initial Generation**::

    ./mame -listcpu > /tmp/cpus.txt
    python3 scripts/build/zbcgen.py --scan-mame /tmp/cpus.txt

Creates initial ``zbc_status.csv`` with all CPUs discovered, automatically
determines header files, and sets initial status (``working`` if header found,
``broken_header`` if not).

**Update from Build Errors**::

    make 2>&1 | tee /tmp/build.log
    python3 scripts/build/zbcgen.py --mark-broken-compile /tmp/build.log

Parses compilation errors and marks failing CPUs as ``broken_compile`` in CSV.

**Update from Validation Errors**::

    ./mame -validate 2>&1 | tee /tmp/validate.log
    python3 scripts/build/zbcgen.py --mark-broken-validate /tmp/validate.log

Parses validation errors and marks failing CPUs as ``broken_validate`` in CSV.

**Regenerate Output Files**::

    python3 scripts/build/zbcgen.py --build

Regenerates ``zbcgen.hpp``, ``zbcgen.ipp``, and updates ``mame.lst`` from
current CSV state without changing status values.

4.2 Workflow
~~~~~~~~~~~~

Typical iterative workflow::

    # 1. Generate fresh list from MAME
    ./mame -listcpu > /tmp/cpus.txt
    python3 scripts/build/zbcgen.py --scan-mame /tmp/cpus.txt

    # 2. Generate source files
    python3 scripts/build/zbcgen.py --build

    # 3. Build and capture errors
    make 2>&1 | tee /tmp/build.log
    python3 scripts/build/zbcgen.py --mark-broken-compile /tmp/build.log
    python3 scripts/build/zbcgen.py --build

    # 4. Rebuild with broken CPUs excluded
    make 2>&1 | tee /tmp/build2.log
    python3 scripts/build/zbcgen.py --mark-broken-compile /tmp/build2.log
    python3 scripts/build/zbcgen.py --build

    # 5. Validate working CPUs
    ./mame -validate 2>&1 | tee /tmp/validate.log
    python3 scripts/build/zbcgen.py --mark-broken-validate /tmp/validate.log
    python3 scripts/build/zbcgen.py --build

    # 6. Final build
    make

4.3 Implementation Details
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Parsing -listcpu Output**:

The enhanced ``-listcpu`` command outputs (on GCC/Clang)::

    Short name:       Device type:      Device class:         Full name:
    m6502             M6502             m6502_device          "MOS Technology 6502"
    z80               Z80               z80_device            "Zilog Z80"

MSVC output omits the device class column::

    Short name:       Device type:      Full name:
    m6502             M6502             "MOS Technology 6502"

The script detects the format and parses accordingly. On MSVC, class names
cannot be inferred, so the script relies on existing CSV data.

**Header File Discovery**:

The script uses a sophisticated device type mapping system:

1. **Load all CPU headers into memory** (``src/devices/cpu/**/*.h``)
2. **Scan for DECLARE_DEVICE_TYPE declarations** using regex pattern::

    DECLARE_DEVICE_TYPE\s*\(\s*(\w+)\s*,

3. **Build mapping** from device type constant to header file path
4. **Look up header** for each CPU's type constant in the mapping

This approach is far more reliable than pattern matching heuristics, as it
directly parses MAME's actual device type declarations. The entire process
loads ~6.3 MB of headers into memory once, then performs in-memory searches.

Example mapping entries::

    M6502 → cpu/m6502/m6502.h
    Z80 → cpu/z80/z80.h
    PENTIUM → cpu/i386/i386.h
    ARM7 → cpu/arm7/arm7.h

CPUs without valid header mappings are marked ``broken_header`` and excluded
from generation.

**Build Error Parsing**:

The script recognizes common GCC/Clang/MSVC error patterns::

    # GCC/Clang
    src/mame/zbc/zbcgen.ipp:123:45: error: 'PENTIUM' was not declared

    # MSVC
    zbcgen.ipp(123): error C2065: 'PENTIUM': undeclared identifier

It extracts the line number, looks up which ``DEFINE_ZBC`` call failed, and
marks that CPU as ``broken_compile`` with the error message in notes.

**Validation Error Parsing**:

The ``mame -validate`` output format::

    Driver zbcm6502 (file zbc.cpp): 1 errors, 0 warnings
    Errors:
    Video screen ':screen' has no refresh rate

Validation errors are stored in the notes field. CPUs with validation errors
are marked ``broken_validate`` and excluded from generated output (commented
out with reason).


5. Integration with MAME Build System
--------------------------------------

5.1 Potential Build Targets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The zbcgen system could potentially be integrated into MAME's build system
with targets similar to::

    # Makefile additions (example - not currently implemented):
    zbcgen-scan:
        @./mame -listcpu > /tmp/cpus.txt
        @python3 scripts/build/zbcgen.py --scan-mame /tmp/cpus.txt

    zbcgen-mark-broken-compile:
        @python3 scripts/build/zbcgen.py --mark-broken-compile $(BUILD_LOG)

    zbcgen-mark-broken-validate:
        @./mame -validate 2>&1 | tee /tmp/validate.log
        @python3 scripts/build/zbcgen.py --mark-broken-validate /tmp/validate.log

    zbcgen-build:
        @python3 scripts/build/zbcgen.py --build

**Note**: These are illustrative examples. Actual build system integration
would need to be coordinated with MAME's existing build infrastructure.

5.2 mame.lst Integration
~~~~~~~~~~~~~~~~~~~~~~~~~

The ``zbcgen.py --build`` command automatically updates ``src/mame/mame.lst``
with the list of working ZBC drivers. The script:

1. Locates the ``@source:zbc/zbc.cpp`` section in mame.lst
2. Replaces the driver list with only CPUs that have:
   - ``status=working``
   - Valid header files (not missing or broken)
3. Sorts drivers alphabetically by shortname
4. Writes the updated list back to mame.lst

This ensures MAME's driver registration stays synchronized with working ZBC
variants. Broken or disabled CPUs are automatically excluded.

5.3 Continuous Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The zbcgen system could enable automated CPU testing in CI pipelines:

1. PR commits trigger build
2. Build failures could be automatically marked in CSV
3. Validation runs on successful builds
4. Status changes tracked in version control
5. Developers fix broken CPUs or mark as disabled with notes

This would ensure the ZBC driver list stays current as MAME evolves.

6. Semihosting Integration
---------------------------

6.1 RIFF-Based Semihosting
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ZBC system includes built-in semihosting support via a memory-mapped device.
Each ZBC variant includes a 32-byte semihost device register block placed
just before video RAM (calculated dynamically based on address space size).

Semihosting is always available - no plugins or command-line options required::

    mame zbcm6502 -elfload program.elf

6.2 Semihost Device Register Map
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The semihost device occupies 32 bytes of address space::

    Offset  Size  Name        Access  Description
    0x00    8     SIGNATURE   R       ASCII "SEMIHOST" (device detection)
    0x08    16    RIFF_PTR    RW      Pointer to RIFF buffer (native endian)
    0x18    1     DOORBELL    W       Write triggers request processing
    0x19    1     STATUS      RW      Interrupt pending (write 0 to clear)
    0x1A-1F 6     (reserved)  -       Padding to 32 bytes

For a 16-bit CPU, the device is at 0xFDE0-0xFDFF.

**STATUS Register (0x19)**:

* **Value 0**: No interrupt pending
* **Value 1**: Timer tick occurred (from SYS_TIMER_CONFIG)
* **Value 2+**: Reserved for future interrupt sources

Write 0 to STATUS to acknowledge the interrupt and deassert the IRQ line.

6.3 Request Flow
~~~~~~~~~~~~~~~~

1. Guest allocates RIFF buffer anywhere in RAM
2. Guest builds RIFF request (CNFG + CALL chunks) in buffer
3. Guest writes buffer address to RIFF_PTR (offset 0x08)
4. Guest writes any value to DOORBELL (offset 0x18)
5. Device processes request synchronously
6. Guest reads response from RIFF buffer

Programs can use semihosting calls to:

* Write debug output to console (SYS_WRITE0, SYS_WRITEC)
* Read/write files on the host filesystem (sandboxed to ~/.mame/semihost/)
* Get system time and clock information
* Configure timer interrupts (SYS_TIMER_CONFIG)
* Exit cleanly

6.4 Timer Interrupts
~~~~~~~~~~~~~~~~~~~~

The semihost device provides a programmable timer via the SYS_TIMER_CONFIG
syscall (opcode 0x32). When enabled, the timer fires at the configured
frequency and:

1. Sets STATUS register to 1
2. Asserts the CPU's IRQ0 line

The guest ISR must:

1. Read STATUS to confirm timer interrupt (value 1)
2. Handle the interrupt
3. Write 0 to STATUS to acknowledge and deassert IRQ

See section 2.3 for detailed timer configuration information.

7. Usage Examples
-----------------

7.1 Running a ZBC System
~~~~~~~~~~~~~~~~~~~~~~~~

::

    # Run 6502 ZBC system (semihosting always available)
    mame zbcm6502 -elfload program.elf

    # Run Z80 ZBC system
    mame zbcz80 -elfload program.elf

    # Run 68000 ZBC system
    mame zbcm68000 -elfload program.elf

The ELF file is loaded at the addresses specified in its program headers and
execution begins at the ELF entry point. The MC6847 display shows system
information on boot, including:
  * CPU name
  * RAM range (0x0 to semihost device)
  * Semihost device address range
  * Video RAM address range

7.2 Writing Test Programs
~~~~~~~~~~~~~~~~~~~~~~~~~~

Example 6502 program using llvm-mos toolchain (displays 'A' on screen)::

    // test.c - Display 'A' on screen using llvm-mos
    // Video RAM at 0xFE00 (16-bit address space)
    // Compile with custom linker script for ZBC memory layout

    volatile char *videoram = (volatile char *)0xFE00;

    int main(void) {
        // Write 'A' to top-left of 32x16 display
        videoram[0] = 'A';

        // Infinite loop
        while (1) {
            // CPU idle
        }

        return 0;
    }

Build and run::

    # Use custom linker script for ZBC memory layout
    # See examples/mame/ in the semihost repository for complete examples
    mame zbcm6502 -elfload test.elf

**Important**: Video RAM address varies by CPU address space size. Check the
boot screen or calculate using formulas in section 2.4. For 16-bit CPUs,
VRAM is at 0xFE00. For 32-bit CPUs, VRAM is at 0xFFFEFE00.

7.3 Automated Testing
~~~~~~~~~~~~~~~~~~~~~

The ZBC system enables automated CPU testing::

    # Test all working ZBC drivers
    for driver in $(grep '^zbc' src/mame/mame.lst | grep -A999 '@source:zbc/zbc.cpp' | grep '^zbc'); do
        echo "Testing $driver..."
        timeout 5 ./mame $driver -elfload test.elf -seconds_to_run 2
    done

This validates that each CPU boots, loads programs, and executes correctly.


8. Known Limitations
--------------------

CPUs excluded from ZBC coverage:

* **Microcontrollers with internal ROM/RAM** - PICs, AVR, 8051, TMS7000, etc.
  These require internal memory regions not provided by external RAM mapping.

* **CPUs requiring dependent devices** - PlayStation 2 VU units, Emotion Engine cores
  These require additional support devices beyond simple RAM/video.

* **Validation failures** - Sony PlayStation CPUs that crash during MAME validation

* **Non-CPU devices** - DMA controllers, graphics interfaces accidentally included

* **Driver name limits** - CPUs whose driver names exceed MAME's 16-character limit


9. Future Enhancements
----------------------

Planned improvements:

* **Performance benchmarking**: Standardized test suite to compare CPU emulation speed
* **Extended semihosting**: Additional syscalls for networking, debugging
* **Conflict resolution**: Better handling of CPUs with conflicting macro definitions
* **Custom configurations**: Support for CPUs requiring special machine_config setup
* **Automated regression testing**: CI integration for detecting CPU emulation bugs
