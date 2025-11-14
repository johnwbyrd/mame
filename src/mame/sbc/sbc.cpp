// license:BSD-3-Clause
// copyright-holders:MAME Team
/***************************************************************************

    Single Board Computer Templates

    Minimal CPU systems for testing and development across all architectures.
    Each SBC includes:
    - RAM (size determined by CPU address width)
    - MC6847 VDG text display (32x16)
    - Quickload support for loading programs

    Design Philosophy:
    This module uses C++ templates to eliminate code duplication across
    CPU architectures. A single template class (sbc_state) is instantiated
    for each CPU type, with compile-time customization of load address,
    clock speed, and video RAM placement. CPU-specific initialization
    (reset vectors, exception tables) is handled via template specialization.

    The DEFINE_SBC macro generates complete machine variants from a single
    line, creating unique classes and registering them with MAME.

    Limitations:
    - No ROM (program must be loaded via -quik)
    - No I/O ports (memory-mapped video only)
    - Video RAM is fixed at calculated address (may conflict with some programs)

    Usage:
    mame sbc6502 -quik program.bin
    mame sbcz80 -quik program.bin
    mame sbc68000 -quik program.bin
    etc.

    The program will be loaded at the configured address (default 0x0200)
    and executed.

***************************************************************************/

#include "emu.h"
#include "emupal.h"
#include "screen.h"

#include "cpu/arm/arm.h"
#include "cpu/arm7/arm7.h"
#include "cpu/h6280/h6280.h"
#include "cpu/i86/i86.h"
#include "cpu/i86/i186.h"
#include "cpu/i86/i286.h"
#include "cpu/i386/i386.h"
#include "cpu/i386/athlon.h"
#include "cpu/m6502/m6502.h"
#include "cpu/m6502/m6510.h"
#include "cpu/m6502/r65c02.h"
#include "cpu/m6502/w65c02s.h"
#include "cpu/m6502/m65ce02.h"
#include "cpu/m6502/g65sc02.h"
#include "cpu/m6502/m4510.h"
#include "cpu/m6502/m8502.h"
#include "cpu/m6502/rp2a03.h"
#include "cpu/m6502/m6507.h"
#include "cpu/m6502/m3745x.h"
#include "cpu/m6502/m5074x.h"
#include "cpu/m6502/st2204.h"
#include "cpu/m6502/xavix.h"
#include "cpu/m6502/deco16.h"
#include "cpu/m6502/w65c02.h"
#include "cpu/m6800/m6800.h"
#include "cpu/m68000/m68000.h"
#include "cpu/m6809/m6809.h"
#include "cpu/mips/mips1.h"
#include "cpu/sh/sh4.h"
#include "cpu/z80/z80.h"

#include "imagedev/snapquik.h"
#include "video/mc6847.h"

namespace {

// Template parameters control compile-time instantiation of CPU-specific variants:
//
// - CPU_TYPE: The CPU device class (e.g., m6502_device, z80_device)
//   Must be a cpu_device subclass. Used for template specialization matching
//   in init_cpu_for_idle<CPU_TYPE, LOAD_ADDR>(). Address space width is
//   queried from this device at runtime.
//
// - LOAD_ADDR: Where quickload programs are loaded (default 0x0200)
//   Default of 0x0200 avoids low memory conflicts (zero page, reset vectors).
//   Must be less than VRAM_ADDR to prevent program/video memory collision.
//
// - CPU_SPEED: CPU clock frequency in Hz (default 10 MHz)
//   Affects emulation speed but not functional behavior. MC6847 video timing
//   is independent (fixed at 4.433619 MHz PAL).
//
// - VRAM_ADDR: Video RAM base address (default 0 = auto-calculate)
//   When 0, VRAM is placed near top of address space with 256-byte gap for
//   vectors/high RAM. Explicit values override auto-calculation.
template <typename CPU_TYPE, uint32_t LOAD_ADDR = 0x0200,
          uint32_t CPU_SPEED = 10'000'000, uint32_t VRAM_ADDR = 0>
class sbc_state : public driver_device {
  public:
	sbc_state(const machine_config &mconfig, device_type type, const char *tag)
	    : driver_device(mconfig, type, tag), m_maincpu(*this, "maincpu"),
	      m_vdg(*this, "vdg"),
	      m_videoram(*this, "videoram", 0x200, ENDIANNESS_LITTLE) {}

	template <typename DEVICE_TYPE>
	void sbc(machine_config &config, DEVICE_TYPE const &cpu_type) ATTR_COLD;

  protected:
	virtual void machine_start() override;
	virtual void machine_reset() override ATTR_COLD;

  private:
	required_device<cpu_device> m_maincpu;
	required_device<mc6847_base_device> m_vdg;

	// Video RAM must be accessible as bytes regardless of CPU bus width.
	// memory_share_creator ensures 8-bit access even on 16/32-bit CPUs,
	// allowing the MC6847 VDG to read character data byte-by-byte.
	// Installed in machine_start() using install_ram() to force byte granularity.
	memory_share_creator<uint8_t> m_videoram;

	void mem_map(address_map &map) ATTR_COLD;
	void init_screen();
	void chrout(char c);
	void print_word(const char *word);
	void print_sentence(const char *text);
	void center_line(const char *text);
	void scroll_up();

	DECLARE_QUICKLOAD_LOAD_MEMBER(quickload_cb);

	uint8_t vdg_videoram_r(offs_t offset);

	// Calculate video RAM address based on CPU address space size.
	// Strategy: Place VRAM near top of address space to maximize contiguous
	// low memory for programs, while reserving 256 bytes above VRAM for
	// CPU-specific vectors (6502 reset/IRQ, Z80 IM2, etc).
	uint32_t get_vram_addr() const {
		if (VRAM_ADDR != 0)
			return VRAM_ADDR; // Explicit override

		// Auto-calculate based on CPU address space size
		if (!m_maincpu || !m_maincpu->has_space(AS_PROGRAM))
			return 0xFD00; // Fallback before CPU initialized

		// Use address mask to get actual address space size (handles CPUs with
		// partial address decoding like 6507's 13-bit space in 16-bit interface)
		uint32_t addr_mask = m_maincpu->space(AS_PROGRAM).addrmask();
		uint32_t max_addr = addr_mask;

		// Place VRAM 512+256 bytes from top of address space
		// (512 for VRAM, 256 gap for vectors/high RAM)
		if (max_addr < 0x400)
			return 0x0000; // Address space too small for this scheme

		uint32_t vram_addr = (max_addr - 0x2FF) & ~0x1FF; // Align to 512-byte boundary

		// Ensure VRAM doesn't overlap LOAD_ADDR
		if (vram_addr <= LOAD_ADDR + 0x200)
			vram_addr = LOAD_ADDR + 0x400; // Place after load area

		return vram_addr;
	}

	uint32_t get_ram_size() const {
		if (!m_maincpu || !m_maincpu->has_space(AS_PROGRAM))
			return 0x10000; // Fallback

		// Use address mask + 1 to get total address space size
		return m_maincpu->space(AS_PROGRAM).addrmask() + 1;
	}

	int m_cursor_pos;
	static constexpr int LINE_WIDTH = 32;
	static constexpr int SCREEN_HEIGHT = 16;
	static constexpr int SCREEN_SIZE = 0x200;
};

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::mem_map(
    address_map &map) {
	map.unmap_value_high(); // Unmapped reads return 0xFF (floating bus)

	uint32_t vram_addr = get_vram_addr();
	uint32_t ram_size = get_ram_size();

	// Map RAM in sections around video RAM (creates fragmented address space)
	if (vram_addr > 0)
		map(0x0000, vram_addr - 1).ram();

	// Video RAM (512 bytes) gap - installed later in machine_start()
	// Must use install_ram() instead of map().ram() to force 8-bit access
	// on CPUs with 16/32-bit bus widths. The MC6847 requires byte-level reads.

	uint32_t after_vram = vram_addr + 0x200;
	if (after_vram < ram_size)
		map(after_vram, ram_size - 1).ram();
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
uint8_t sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::vdg_videoram_r(
    offs_t offset) {
	// MC6847 VDG reads character codes from video RAM for display.
	// Called ~15,000 times/second as VDG scans the 32x16 character grid.
	return m_videoram[offset & 0x1ff]; // Mask to 512-byte range
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::scroll_up() {
	for (int i = 0; i < SCREEN_SIZE - LINE_WIDTH; i++)
		m_videoram[i] = m_videoram[i + LINE_WIDTH];

	for (int i = SCREEN_SIZE - LINE_WIDTH; i < SCREEN_SIZE; i++)
		m_videoram[i] = 0x20;

	m_cursor_pos -= LINE_WIDTH;
	if (m_cursor_pos < 0)
		m_cursor_pos = 0;
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::chrout(char c) {
	if (c == '\r' || c == '\n') {
		m_cursor_pos = ((m_cursor_pos / LINE_WIDTH) + 1) * LINE_WIDTH;
		if (m_cursor_pos >= SCREEN_SIZE) {
			scroll_up();
			m_cursor_pos = SCREEN_SIZE - LINE_WIDTH;
		}
	} else {
		m_videoram[m_cursor_pos] = toupper(c);
		m_cursor_pos++;
		if (m_cursor_pos >= SCREEN_SIZE) {
			scroll_up();
			m_cursor_pos = SCREEN_SIZE - LINE_WIDTH;
		}
	}
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::print_word(
    const char *word) {
	int word_len = strlen(word);
	int col = m_cursor_pos % LINE_WIDTH;

	int needed = word_len;
	if (col > 0)
		needed++;

	if (col > 0 && col + needed > LINE_WIDTH) {
		chrout('\r');
		col = 0;
	}

	if (col > 0)
		chrout(' ');

	for (int i = 0; i < word_len; i++)
		chrout(word[i]);
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::print_sentence(
    const char *text) {
	char word[64];
	int word_idx = 0;

	for (const char *p = text; *p; p++) {
		if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
			if (word_idx > 0) {
				word[word_idx] = '\0';
				print_word(word);
				word_idx = 0;
			}
		} else {
			if (word_idx < 63)
				word[word_idx++] = *p;
		}
	}

	if (word_idx > 0) {
		word[word_idx] = '\0';
		print_word(word);
	}
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::center_line(
    const char *text) {
	int len = strlen(text);
	int padding = (LINE_WIDTH - len) / 2;

	for (int i = 0; i < padding; i++)
		chrout(' ');

	for (int i = 0; i < len; i++)
		chrout(text[i]);

	chrout('\r');
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::init_screen() {
	for (int i = 0; i < 0x200; i++)
		m_videoram[i] = 0x20;

	m_cursor_pos = 0;

	center_line("Single board computer");
	center_line(m_maincpu->name());
	center_line("");
	center_line("github.com/johnwbyrd/semihost");
	center_line("");

	// Display memory configuration
	char addr_buf[64];
	uint32_t vram_addr = get_vram_addr();
	uint32_t available_ram = vram_addr - LOAD_ADDR;

	snprintf(addr_buf, sizeof(addr_buf), "Load address: 0x%X", LOAD_ADDR);
	center_line(addr_buf);

	snprintf(addr_buf, sizeof(addr_buf), "Available RAM: %u bytes",
	         available_ram);
	center_line(addr_buf);

	snprintf(addr_buf, sizeof(addr_buf), "Video RAM: 0x%X-0x%X",
	         vram_addr, vram_addr + 0x1FF);
	center_line(addr_buf);

	center_line("");

	print_sentence("This is a minimal system with RAM and a "
	               "MC6847 video display. "
	               "Load a headerless binary in MAME using the -quik option. "
	               "The program will be loaded and executed. "
	               "Happy coding!");
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::machine_start() {
	// Install 8-bit video RAM at the calculated address.
	// CRITICAL: Must use install_ram() here (not map().ram() in mem_map())
	// to force byte-granular access on 16/32-bit CPUs. The MC6847 VDG
	// always reads bytes, but map().ram() creates CPU-width memory.
	uint32_t vram_addr = get_vram_addr();
	m_maincpu->space(AS_PROGRAM)
	    .install_ram(vram_addr, vram_addr + 0x1FF, m_videoram.target());
}

// CPU-specific idle initialization using template specialization.
//
// This uses C++ template specialization to provide CPU-specific boot code
// while maintaining a generic interface. The compiler selects the most
// specific matching template at compile time based on CPU_TYPE and LOAD_ADDR.
//
// Each specialization writes reset vectors and idle loops appropriate for
// the CPU architecture, allowing the system to boot and wait for quickload.
template <typename CPU_TYPE, uint32_t LOAD_ADDR>
void init_cpu_for_idle(address_space &space) {
	// Default: no special initialization needed for CPUs without specialization
}

// 6502 specialization: Set reset vector and create idle loop
template <> void init_cpu_for_idle<m6502_device, 0x0200>(address_space &space) {
	// 6502 reads reset vector from 0xFFFC-0xFFFD (little-endian)
	space.write_byte(0xfffc, 0x00);
	space.write_byte(0xfffd, 0x02); // Points to 0x0200

	// Idle loop at load address: JMP $0200 (infinite loop)
	space.write_byte(0x0200, 0x4c); // JMP absolute opcode
	space.write_byte(0x0201, 0x00);
	space.write_byte(0x0202, 0x02);
}

// Z80 specialization: Boot code and NMI handler
template <> void init_cpu_for_idle<z80_device, 0x0200>(address_space &space) {
	// Z80 starts at 0x0000 on reset, jump to load address
	space.write_byte(0x0000, 0xc3); // JP opcode
	space.write_byte(0x0001, 0x00);
	space.write_byte(0x0002, 0x02); // JP $0200

	// NMI handler at 0x0066 (MC6847 vsync generates NMI)
	space.write_byte(0x0066, 0xed); // RETN opcode
	space.write_byte(0x0067, 0x45); // (2-byte instruction)

	// Idle loop: JR -2 (relative jump to self)
	space.write_byte(0x0200, 0x18); // JR opcode
	space.write_byte(0x0201, 0xfe); // Offset -2
}

// 68000 specialization: Exception vectors and idle loop
template <>
void init_cpu_for_idle<m68000_device, 0x0200>(address_space &space) {
	// 68000 reads initial SP from 0x0000-0x0003 (big-endian)
	space.write_byte(0x0000, 0x00);
	space.write_byte(0x0001, 0x00);
	space.write_byte(0x0002, 0xFF);
	space.write_byte(0x0003, 0xF0); // SP = 0x0000FFF0

	// 68000 reads initial PC from 0x0004-0x0007 (big-endian)
	space.write_byte(0x0004, 0x00);
	space.write_byte(0x0005, 0x00);
	space.write_byte(0x0006, 0x02);
	space.write_byte(0x0007, 0x00); // PC = 0x00000200

	// Idle loop: BRA.S -2 (branch to self)
	space.write_byte(0x0200, 0x60); // BRA.S opcode
	space.write_byte(0x0201, 0xfe); // Displacement -2
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::machine_reset() {
	init_screen();

	address_space &space = m_maincpu->space(AS_PROGRAM);

	// Call CPU-specific initialization
	init_cpu_for_idle<CPU_TYPE, LOAD_ADDR>(space);
}

// Quickload callback: Load binary program into memory
// Called when user specifies -quik program.bin on command line.
// Loads raw binary at LOAD_ADDR and prepares system for execution.
template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
std::pair<std::error_condition, std::string>
sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::quickload_cb(
    snapshot_image_device &image) {
	uint32_t size = image.length();
	uint32_t vram_addr = get_vram_addr();

	// Validate program fits between LOAD_ADDR and video RAM
	if (size > vram_addr - LOAD_ADDR)
		return std::make_pair(image_error::INVALIDLENGTH, "Program too large");

	// Read into temporary buffer (can't read directly to fragmented address space)
	std::vector<uint8_t> program(size);
	if (image.fread(&program[0], size) != size)
		return std::make_pair(image_error::UNSPECIFIED,
		                      "Failed to read program file");

	// Write program to memory byte-by-byte (handles endianness/bus width)
	address_space &space = m_maincpu->space(AS_PROGRAM);
	for (uint32_t i = 0; i < size; i++)
		space.write_byte(LOAD_ADDR + i, program[i]);

	// Clear screen to remove boot message
	init_screen();

	return std::make_pair(std::error_condition(), std::string()); // Success
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
template <typename DEVICE_TYPE>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::sbc(
    machine_config &config, DEVICE_TYPE const &cpu_type) {
	cpu_type(config, m_maincpu, CPU_SPEED);
	m_maincpu->set_addrmap(AS_PROGRAM, &sbc_state::mem_map);

	SCREEN(config, "screen", SCREEN_TYPE_RASTER);

	MC6847(config, m_vdg, 4.433619_MHz_XTAL, true); // PAL mode
	m_vdg->set_screen("screen");
	m_vdg->fsync_wr_callback().set_inputline(m_maincpu, INPUT_LINE_NMI);
	m_vdg->input_callback().set(FUNC(sbc_state::vdg_videoram_r));

	QUICKLOAD(config, "quickload", "bin")
	    .set_load_callback(FUNC(sbc_state::quickload_cb));
}

} // anonymous namespace

// Macro to define a complete SBC variant in a single line.
//
// This macro generates:
// 1. A derived class (sbc_<name>_state) inheriting from sbc_state template
// 2. ROM definition (empty for these systems)
// 3. MAME machine registration (makes it available via command line)
//
// Parameters:
//   cpu_class: CPU device class for template instantiation (e.g., m6502_device)
//   cpu_type: MAME device type macro for machine_config (e.g., M6502)
//   short_name: Machine name suffix, creates "sbc<short_name>" (e.g., 6502 → sbc6502)
//   display_name: Human-readable name for UI (e.g., "MOS 6502")
//   ...: Optional template parameter overrides (load_addr, cpu_speed, vram_addr)
//        Example: DEFINE_SBC(..., ..., ..., ..., 0x1000, 8000000) sets LOAD_ADDR=0x1000, CPU_SPEED=8MHz
#define DEFINE_SBC(cpu_class, cpu_type, short_name, display_name, ...)         \
	namespace {                                                                \
	class sbc_##short_name##_state                                             \
	    : public sbc_state<cpu_class, ##__VA_ARGS__> {                         \
	  public:                                                                  \
		using sbc_state<cpu_class, ##__VA_ARGS__>::sbc_state;                  \
		void machine_config(machine_config &config) { sbc(config, cpu_type); } \
	};                                                                         \
	}                                                                          \
	ROM_START(sbc##short_name)                                                 \
	ROM_END                                                                    \
	COMP(2025, sbc##short_name, 0, 0, machine_config, 0,                       \
	     sbc_##short_name##_state, empty_init, "MAME",                         \
	     "Single Board Computer - " display_name, MACHINE_NO_SOUND_HW)

// Define SBC variants for major CPU architectures.
// Each line creates a complete emulated machine accessible via command line.
// CPUs with init_cpu_for_idle specializations (6502, Z80, 68000) are fully functional.
// CPUs without specializations may require additional boot code for proper initialization.

// Original 6 CPUs
DEFINE_SBC(m6502_device, M6502, 6502, "MOS 6502")
DEFINE_SBC(z80_device, Z80, z80, "Zilog Z80")
DEFINE_SBC(m68000_device, M68000, 68000, "Motorola 68000")
DEFINE_SBC(arm7_cpu_device, ARM7, arm7, "ARM7")
DEFINE_SBC(i386_device, I386, i386, "Intel 80386")
DEFINE_SBC(r3000a_device, R3000A, mips, "MIPS R3000A")

// Intel/AMD x86 Family (17 new)
DEFINE_SBC(i8086_cpu_device, I8086, i8086, "Intel 8086")
DEFINE_SBC(i8088_cpu_device, I8088, i8088, "Intel 8088")
DEFINE_SBC(i80186_cpu_device, I80186, i80186, "Intel 80186")
DEFINE_SBC(i80188_cpu_device, I80188, i80188, "Intel 80188")
DEFINE_SBC(i80286_cpu_device, I80286, i80286, "Intel 80286")
DEFINE_SBC(i386sx_device, I386SX, i386sx, "Intel 386SX")
DEFINE_SBC(i486_device, I486, i486, "Intel 486")
DEFINE_SBC(i486dx4_device, I486DX4, i486dx4, "Intel 486DX4")
DEFINE_SBC(pentium_device, PENTIUM, pentium, "Intel Pentium")
DEFINE_SBC(pentium_mmx_device, PENTIUM_MMX, pentmmx, "Intel Pentium MMX")
DEFINE_SBC(pentium_pro_device, PENTIUM_PRO, pentpro, "Intel Pentium Pro")
DEFINE_SBC(pentium2_device, PENTIUM2, pent2, "Intel Pentium II")
DEFINE_SBC(pentium3_device, PENTIUM3, pent3, "Intel Pentium III")
DEFINE_SBC(pentium4_device, PENTIUM4, pent4, "Intel Pentium 4")
DEFINE_SBC(mediagx_device, MEDIAGX, mediagx, "Cyrix MediaGX")
DEFINE_SBC(am186em_device, AM186EM, am186em, "AMD Am186EM")
DEFINE_SBC(athlonxp_device, ATHLONXP, athlonxp, "AMD Athlon XP")

// 6502 Variants (15 new)
DEFINE_SBC(m6510_device, M6510, m6510, "MOS 6510 (C64)")
DEFINE_SBC(r65c02_device, R65C02, r65c02, "WDC 65C02")
DEFINE_SBC(w65c02s_device, W65C02S, w65c02s, "WDC W65C02S")
DEFINE_SBC(m65ce02_device, M65CE02, m65ce02, "CSG 65CE02")
DEFINE_SBC(g65sc02_device, G65SC02, g65sc02, "Rockwell G65SC02")
DEFINE_SBC(m4510_device, M4510, m4510, "CSG 4510 (C65)")
DEFINE_SBC(m8502_device, M8502, m8502, "CSG 8502 (C128)")
DEFINE_SBC(rp2a03_device, RP2A03, rp2a03, "Ricoh RP2A03 (NES)")
// M6507 and M50740 have 13-bit address spaces - these are disabled due to address space limitations
// DEFINE_SBC(m6507_device, M6507, m6507, "MOS 6507 (Atari 2600)", 0x0200, 10'000'000, 0x1400)
DEFINE_SBC(m37450_device, M37450, m37450, "Mitsubishi M37450")
// DEFINE_SBC(m50740_device, M50740, m50740, "Mitsubishi M50740", 0x0200, 10'000'000, 0x1400)
DEFINE_SBC(st2204_device, ST2204, st2204, "Sitronix ST2204")
DEFINE_SBC(xavix_device, XAVIX, xavix, "SSD XaviX")
DEFINE_SBC(deco16_device, DECO16, deco16, "Data East DECO16")

// ARM Variants (8 new)
DEFINE_SBC(arm_cpu_device, ARM, arm, "ARM")
DEFINE_SBC(arm9_cpu_device, ARM9, arm9, "ARM9")
DEFINE_SBC(arm920t_cpu_device, ARM920T, arm920t, "ARM920T")
DEFINE_SBC(arm946es_cpu_device, ARM946ES, arm946es, "ARM946ES")
DEFINE_SBC(arm11_cpu_device, ARM11, arm11, "ARM11")
DEFINE_SBC(arm1176jzf_s_cpu_device, ARM1176JZF_S, arm1176, "ARM1176JZF-S")
DEFINE_SBC(pxa255_cpu_device, PXA255, pxa255, "Intel XScale PXA255")
DEFINE_SBC(sa1110_cpu_device, SA1110, sa1110, "StrongARM SA-1110")

// Other Important CPUs (4 new)
DEFINE_SBC(m6800_cpu_device, M6800, m6800, "Motorola 6800")
DEFINE_SBC(mc6809e_device, MC6809E, mc6809e, "Motorola 6809E")
DEFINE_SBC(h6280_device, H6280, h6280, "Hudson H6280")
DEFINE_SBC(sh4_device, SH4, sh4, "Hitachi SuperH-4")
