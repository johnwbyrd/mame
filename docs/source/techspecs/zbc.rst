Zero Board Computer (ZBC)
==========================

.. contents:: :local:


1. Overview
-----------

1.1 Purpose
~~~~~~~~~~~

The ZBC (Zero Board Computer) system provides minimal, standardized test
environments for all emulatable CPU architectures in MAME. Each ZBC variant
consists of a CPU, RAM, and text display, allowing programs to be loaded and
executed via quickload.

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
compile-time customization of load address, clock speed, and video RAM
placement. CPU-specific initialization (reset vectors, exception tables)
is handled via template specialization.

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
  * ``src/mame/zbc/zbcgen.cpp`` - All ``DEFINE_ZBC()`` macro invocations

**Build Tools**:
  * ``scripts/build/zbcgen.py`` - Main generator script
  * ``src/mame/zbc/zbc_status.csv`` - Knowledge base tracking CPU status (version controlled)

**MAME Infrastructure**:
  * Enhanced ``-listcpu`` command - Outputs CPU metadata
  * Enhanced device type system - Tracks type constant names at runtime

2.2 ZBC Template System
~~~~~~~~~~~~~~~~~~~~~~~

Each ZBC system includes:

* **RAM**: Sized automatically based on CPU address space width
* **MC6847 VDG**: Text display (32x16 characters) at top of address space
* **Quickload**: Support for loading headerless binary programs
* **CPU Init**: Architecture-specific boot code (reset vectors, etc.)

Memory Layout (typical 16-bit CPU)::

    0x0000-0x01FF   Low memory (zero page, vectors, stack)
    0x0200-0xFBFF   Available RAM (load address to semihost)
    0xFC00-0xFDFF   Semihosting buffer (1024 bytes)
    0xFE00-0xFFFF   Video RAM (512 bytes)

The system scales automatically for different address space sizes (8-bit through
64-bit CPUs).

2.3 DEFINE_ZBC Macro
~~~~~~~~~~~~~~~~~~~~

The ``DEFINE_ZBC`` macro in ``zbc.cpp`` generates a complete ZBC variant::

    DEFINE_ZBC(m6502_device, M6502, 6502, "MOS 6502")

This expands to:

1. A derived class (``zbc_6502_state``) inheriting from ``zbc_state<m6502_device>``
2. ROM definition (empty for ZBC systems)
3. MAME machine registration (``COMP`` macro)

Parameters:
  * ``cpu_class``: CPU device class for template instantiation (e.g., ``m6502_device``)
  * ``cpu_type``: MAME device type macro for machine_config (e.g., ``M6502``)
  * ``short_name``: Machine name suffix, creates ``zbc<short_name>`` (e.g., ``6502`` → ``zbc6502``)
  * ``display_name``: Human-readable name for UI (e.g., ``"MOS 6502"``)
  * ``...``: Optional template parameter overrides (load_addr, cpu_speed, vram_addr)

Optional parameters allow customization::

    DEFINE_ZBC(pdp1_device, PDP1, pdp1, "DEC PDP-1", 0x0010, 200000)
    // Sets LOAD_ADDR=0x0010, CPU_SPEED=200kHz


3. Knowledge Base (zbc_status.csv)
----------------------------------

3.1 Schema
~~~~~~~~~~

The CSV file serves as the single source of truth for CPU status::

    shortname,type_constant,class_name,fullname,status,header_file,notes

Fields:
  * ``shortname``: Short identifier (e.g., ``"pentium"``)
  * ``type_constant``: Device type constant (e.g., ``"PENTIUM"``)
  * ``class_name``: C++ device class name (e.g., ``"pentium_device"``)
  * ``fullname``: Human-readable name (e.g., ``"Intel Pentium"``)
  * ``status``: Current state (see 3.2)
  * ``header_file``: Include path (e.g., ``"cpu/i386/i386.h"``)
  * ``notes``: Freeform text (error messages, reasons for disabled status)

3.2 Status Values
~~~~~~~~~~~~~~~~~

* ``working``: CPU compiles, validates, and runs correctly
* ``broken_compile``: Compilation fails (syntax errors, missing headers, etc.)
* ``broken_validate``: Compiles but fails ``mame -validate`` checks
* ``disabled``: Manually disabled (requires special configuration, conflicting macros, etc.)
* ``unknown``: Not yet tested (initial state)

3.3 Example Entries
~~~~~~~~~~~~~~~~~~~

::

    m6502,M6502,m6502_device,"MOS 6502",working,cpu/m6502/m6502.h,
    pentium,PENTIUM,pentium_device,"Intel Pentium",working,cpu/i386/i386.h,
    alto2,ALTO2,alto2_cpu_device,"Xerox Alto II",broken_validate,cpu/alto2/alto2cpu.h,validation fails: unknown error
    v30mz,V30MZ,v30mz_device,"NEC V30MZ",disabled,cpu/v30mz/v30mz.h,conflicts with nec.h (enum NEC_PC)


4. zbcgen.py Tool
-----------------

4.1 Command-Line Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``zbcgen.py`` script supports multiple operation modes:

**Initial Generation**::

    ./mame -listcpu > /tmp/cpus.txt
    zbcgen.py --generate /tmp/cpus.txt

Creates initial ``zbc_status.csv`` with all CPUs marked as ``unknown``, then
generates ``zbcgen.hpp`` and ``zbcgen.cpp``.

**Update from Build Errors**::

    make 2>&1 | tee /tmp/build.log
    zbcgen.py --update-build /tmp/build.log

Parses compilation errors and marks failing CPUs as ``broken_compile`` in CSV.

**Update from Validation Errors**::

    ./mame -validate 2>&1 | tee /tmp/validate.log
    zbcgen.py --update-validate /tmp/validate.log

Parses validation errors and marks failing CPUs as ``broken_validate`` in CSV.

**Regenerate Output Files**::

    zbcgen.py --regenerate

Regenerates ``zbcgen.hpp`` and ``zbcgen.cpp`` from current CSV state without
changing status values.

4.2 Workflow
~~~~~~~~~~~~

Typical iterative workflow::

    # 1. Generate fresh list from MAME (or edit CSV manually)
    ./mame -listcpu > /tmp/cpus.txt
    zbcgen.py --generate /tmp/cpus.txt

    # 3. Build and capture errors
    make 2>&1 | tee /tmp/build.log
    zbcgen.py --update-build /tmp/build.log
    zbcgen.py --regenerate

    # 4. Rebuild with broken CPUs excluded
    make 2>&1 | tee /tmp/build2.log
    zbcgen.py --update-build /tmp/build2.log
    zbcgen.py --regenerate

    # 5. Validate working CPUs
    ./mame -validate 2>&1 | tee /tmp/validate.log
    zbcgen.py --update-validate /tmp/validate.log
    zbcgen.py --regenerate

    # 6. Final build
    make

4.3 Implementation Details
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Parsing -listcpu Output**:

The enhanced ``-listcpu`` command outputs (on GCC/Clang)::

    Short name:       Device type:      Device class:         Full name:
    pentium           PENTIUM           pentium_device        "Intel Pentium"
    z80               Z80               z80_device            "Zilog Z80"

MSVC output omits the device class column::

    Short name:       Device type:      Full name:
    pentium           PENTIUM           "Intel Pentium"

The script detects the format and parses accordingly.

**Header File Discovery**:

Three strategies (in priority order):

1. **Explicit mapping**: CSV already contains header_file from previous run
2. **Pattern matching**: Derive from class name (e.g., ``pentium_device`` → ``cpu/i386/i386.h``)
3. **Source scanning**: Parse existing ``zbc.cpp`` includes and match by class name

The pattern matching uses heuristics::

    # Example patterns:
    m6502_device     → cpu/m6502/m6502.h
    z80_device       → cpu/z80/z80.h
    i386_device      → cpu/i386/i386.h
    arm7_cpu_device  → cpu/arm7/arm7.h

For ambiguous cases, the script searches ``src/mame/zbc/zbc.cpp`` for existing
includes and builds a mapping.

**Build Error Parsing**:

The script recognizes common GCC/Clang/MSVC error patterns::

    # GCC/Clang
    src/mame/zbc/zbcgen.cpp:123:45: error: 'PENTIUM' was not declared

    # MSVC
    zbcgen.cpp(123): error C2065: 'PENTIUM': undeclared identifier

It extracts the line number, looks up which ``DEFINE_ZBC`` call failed, and
marks that CPU as ``broken_compile`` with the error message in notes.

**Validation Error Parsing**:

The ``mame -validate`` output format::

    Driver zbc6502 (file zbc.cpp): 1 errors, 0 warnings
    Errors:
    Video screen ':screen' has no refresh rate

Validation errors are stored in the notes field. CPUs with validation errors
are marked ``broken_validate`` and excluded from generated output (commented
out with reason).


5. Generated File Format
------------------------

5.1 zbcgen.hpp
~~~~~~~~~~~~~~

Auto-generated header file with all CPU includes::

    // Auto-generated by zbcgen.py - DO NOT EDIT
    // Generated: 2025-11-18 17:30:45
    // Source: zbc_status.csv (working CPUs only)

    // Classic 8-bit CPUs
    #include "cpu/i8085/i8085.h"           // Intel 8080/8085
    #include "cpu/m6502/m6502.h"           // MOS 6502 family
    #include "cpu/m6800/m6800.h"           // Motorola 6800 family
    #include "cpu/z80/z80.h"               // Zilog Z80 family

    // 16-bit CPUs
    #include "cpu/i86/i86.h"               // Intel 8086 family
    // ... (alphabetically sorted by family)

Includes are:
  * Generated only for CPUs with ``status=working``
  * Grouped by architecture family (extracted from comments in original zbc.cpp)
  * Sorted alphabetically within each group
  * Include both path and description comment

5.2 zbcgen.cpp
~~~~~~~~~~~~~~

Auto-generated implementation file::

    // Auto-generated by zbcgen.py - DO NOT EDIT
    // Generated: 2025-11-18 17:30:45
    // Source: zbc_status.csv
    //
    // Statistics:
    //   Total CPUs discovered: 847
    //   Working: 623
    //   Broken (compile): 89
    //   Broken (validate): 42
    //   Disabled: 93

    #include "emu.h"
    #include "zbcgen.hpp"

    // Classic 8-bit CPUs (152 CPUs)
    DEFINE_ZBC(m6502_device, M6502, 6502, "MOS 6502")
    DEFINE_ZBC(z80_device, Z80, z80, "Zilog Z80")
    // ...

    // Broken CPUs (commented out with reason)
    // DEFINE_ZBC(alto2_cpu_device, ALTO2, alto2, "Xerox Alto II")
    //   Status: broken_validate
    //   Error: validation fails: unknown error

Format:
  * Header with generation timestamp and statistics
  * Working CPUs grouped by family
  * Broken/disabled CPUs commented out with status explanation
  * Line numbers preserved for error correlation


6. Integration with MAME Build System
--------------------------------------

6.1 Build Targets
~~~~~~~~~~~~~~~~~

The zbcgen system integrates into MAME's build system::

    # Makefile additions:
    zbcgen-extract:
        @python3 scripts/build/zbcgen.py --extract-current

    zbcgen-generate:
        @./mame -listcpu > /tmp/cpus.txt
        @python3 scripts/build/zbcgen.py --generate /tmp/cpus.txt

    zbcgen-update-build:
        @python3 scripts/build/zbcgen.py --update-build $(BUILD_LOG)

    zbcgen-update-validate:
        @./mame -validate 2>&1 | tee /tmp/validate.log
        @python3 scripts/build/zbcgen.py --update-validate /tmp/validate.log

    zbcgen-regenerate:
        @python3 scripts/build/zbcgen.py --regenerate

6.2 Continuous Integration
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The zbcgen system enables automated CPU testing in CI pipelines:

1. PR commits trigger build
2. Build failures update ``zbc_status.csv`` automatically
3. Validation runs on successful builds
4. Status changes are reported in PR comments
5. Developers fix broken CPUs or mark as disabled

This ensures the ZBC driver list stays current as MAME evolves.


7. CPU-Specific Initialization
-------------------------------

7.1 Template Specialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some CPUs require specific boot code to enter an idle loop. The ``init_cpu_for_idle``
function uses template specialization::

    template <typename CPU_TYPE, uint32_t LOAD_ADDR>
    void init_cpu_for_idle(address_space &space) {
        // Default: no initialization
    }

    // 6502 specialization
    template <>
    void init_cpu_for_idle<m6502_device, 0x0200>(address_space &space) {
        // Set reset vector to 0x0200
        space.write_byte(0xfffc, 0x00);
        space.write_byte(0xfffd, 0x02);

        // Idle loop at 0x0200: JMP $0200
        space.write_byte(0x0200, 0x4c);  // JMP absolute
        space.write_byte(0x0201, 0x00);
        space.write_byte(0x0202, 0x02);
    }

Specializations exist for:
  * MOS 6502 family (reset vector at 0xFFFC)
  * Zilog Z80 family (boot at 0x0000, NMI handler at 0x0066)
  * Motorola 68000 family (vector table at 0x0000)

7.2 Adding New Specializations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To add CPU-specific initialization:

1. Identify CPU requirements (reset vector address, boot sequence)
2. Add template specialization in ``zbc.cpp``::

    template <>
    void init_cpu_for_idle<my_cpu_device, 0x0200>(address_space &space) {
        // CPU-specific boot code
    }

3. Rebuild and test with ``mame zbcmycpu -quik test.bin``

CPUs without specializations will boot with uninitialized memory. They may
require additional boot code loaded via quickload.


8. Usage Examples
-----------------

8.1 Running a ZBC System
~~~~~~~~~~~~~~~~~~~~~~~~

::

    # Run 6502 ZBC system
    mame zbc6502 -quik program.bin

    # Run Z80 ZBC system at 4MHz
    mame zbcz80 -quik program.bin

    # Run 68000 ZBC system
    mame zbc68000 -quik program.bin

The program will be loaded at the configured address (default 0x0200) and
executed. The MC6847 display shows system information on boot.

8.2 Writing Test Programs
~~~~~~~~~~~~~~~~~~~~~~~~~~

Example 6502 program (displays 'A' on screen)::

    ; Video RAM at 0xFE00
    ; Display is 32x16 = 512 bytes

        LDA #$41        ; ASCII 'A'
        STA $FE00       ; Write to top-left of screen
    loop:
        JMP loop        ; Infinite loop

Assemble to raw binary and load::

    vasm6502_oldstyle -Fbin -o test.bin test.asm
    mame zbc6502 -quik test.bin

8.3 Automated Testing
~~~~~~~~~~~~~~~~~~~~~

The ZBC system enables automated CPU testing::

    # Test all working CPUs
    for cpu in $(./mame -listmedia | grep '^zbc' | cut -d' ' -f1); do
        echo "Testing $cpu..."
        timeout 5 ./mame $cpu -quik test.bin -seconds_to_run 2
    done

This validates that each CPU boots, loads programs, and executes correctly.


9. Future Enhancements
----------------------

9.1 Planned Features
~~~~~~~~~~~~~~~~~~~~

* **Semihosting support**: CPU-to-host I/O for printf debugging
* **Performance benchmarking**: Standardized test suite across all CPUs
* **Header auto-discovery**: Scan MAME source tree to map classes to headers
* **Conflict resolution**: Detect and handle CPUs with conflicting macro definitions
* **Custom configurations**: Support CPUs requiring special machine_config setup

9.2 Long-term Vision
~~~~~~~~~~~~~~~~~~~~

The ZBC system aims to provide:

* Complete CPU coverage (800+ CPU variants)
* Automated regression testing for CPU cores
* Performance comparison across architectures
* Educational resource for CPU emulation
* Foundation for cross-architecture tooling


10. References
--------------

* ``src/mame/zbc/zbc.cpp`` - ZBC template implementation
* ``src/frontend/mame/clifront.cpp`` - Enhanced -listcpu command
* ``src/emu/device.h`` - Device type system with typename support
* ``plan.md`` - Original ZBC canonical CPU support plan
