// license:BSD-3-Clause
// copyright-holders:MAME Team
/***************************************************************************

    Single Board Computer Templates

    Minimal CPU systems for testing and development across all architectures.
    Each SBC includes:
    - RAM (size determined by CPU address width)
    - MC6847 VDG text display (32x16)
    - Quickload support for loading programs

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

#include "cpu/arm7/arm7.h"
#include "cpu/i386/i386.h"
#include "cpu/m6502/m6502.h"
#include "cpu/m68000/m68000.h"
#include "cpu/mips/mips1.h"
#include "cpu/z80/z80.h"

#include "imagedev/snapquik.h"
#include "video/mc6847.h"

namespace {

// Template parameters:
// - CPU_TYPE: The CPU device class (e.g., m6502_device)
// - LOAD_ADDR: Where to load programs (default 0x0200)
// - CPU_SPEED: CPU clock speed in Hz (default 10 MHz)
// - VRAM_ADDR: Video RAM address (0 = auto-calculate from top of address space)
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

	// Calculate video RAM address based on CPU address width
	uint32_t get_vram_addr() const {
		if (VRAM_ADDR != 0)
			return VRAM_ADDR;

		// Auto-calculate: place VRAM near top of address space
		// Leave 256 bytes for high RAM/vectors
		if (!m_maincpu || !m_maincpu->has_space(AS_PROGRAM))
			return 0xFD00; // Fallback for 16-bit

		uint8_t addr_bits = m_maincpu->space(AS_PROGRAM).addr_width();

		if (addr_bits <= 16)
			return 0xFD00; // 16-bit: 0xFD00-0xFEFF, high RAM at 0xFF00-0xFFFF
		else if (addr_bits <= 24)
			return 0xFFFD00; // 24-bit
		else
			return 0xFFFFFD00; // 32-bit+
	}

	uint32_t get_ram_size() const {
		if (!m_maincpu || !m_maincpu->has_space(AS_PROGRAM))
			return 0x10000; // Fallback

		uint8_t addr_bits = m_maincpu->space(AS_PROGRAM).addr_width();
		return (1ULL << addr_bits);
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
	map.unmap_value_high();

	uint32_t vram_addr = get_vram_addr();
	uint32_t ram_size = get_ram_size();

	// Map RAM in sections around video RAM
	if (vram_addr > 0)
		map(0x0000, vram_addr - 1).ram();

	// Video RAM (512 bytes) - will be installed in machine_start()
	// Leaving this unmapped so we can install 8-bit RAM explicitly

	uint32_t after_vram = vram_addr + 0x200;
	if (after_vram < ram_size)
		map(after_vram, ram_size - 1).ram();
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
uint8_t sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::vdg_videoram_r(
    offs_t offset) {
	return m_videoram[offset & 0x1ff];
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

	print_sentence("This is a minimal system with RAM and a "
	               "MC6847 video display. "
	               "Load a headerless binary in MAME using the -quik option. "
	               "The program will be loaded and executed. "
	               "Happy coding!");
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::machine_start() {
	// Install 8-bit video RAM at the calculated address
	// This ensures videoram is always byte-accessible regardless of CPU bus
	// width
	uint32_t vram_addr = get_vram_addr();
	m_maincpu->space(AS_PROGRAM)
	    .install_ram(vram_addr, vram_addr + 0x1FF, m_videoram.target());
}

// CPU-specific idle initialization - default (do nothing)
template <typename CPU_TYPE, uint32_t LOAD_ADDR>
void init_cpu_for_idle(address_space &space) {
	// Default: no special initialization needed
}

// Z80 specialization
template <> void init_cpu_for_idle<z80_device, 0x0200>(address_space &space) {
	// Reset vector: JP 0x0200
	space.write_byte(0x0000, 0xc3); // JP opcode
	space.write_byte(0x0001, 0x00);
	space.write_byte(0x0002, 0x02);

	// NMI handler: RETN
	space.write_byte(0x0066, 0xed);
	space.write_byte(0x0067, 0x45);

	// Idle loop: JR -2
	space.write_byte(0x0200, 0x18);
	space.write_byte(0x0201, 0xfe);
}

// 68000 specialization
template <>
void init_cpu_for_idle<m68000_device, 0x0200>(address_space &space) {
	// Initial SP (big-endian)
	space.write_byte(0x0000, 0x00);
	space.write_byte(0x0001, 0x00);
	space.write_byte(0x0002, 0xFF);
	space.write_byte(0x0003, 0xF0);

	// Initial PC (big-endian)
	space.write_byte(0x0004, 0x00);
	space.write_byte(0x0005, 0x00);
	space.write_byte(0x0006, 0x02);
	space.write_byte(0x0007, 0x00);

	// Idle loop: BRA.S -2
	space.write_byte(0x0200, 0x60);
	space.write_byte(0x0201, 0xfe);
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::machine_reset() {
	init_screen();

	address_space &space = m_maincpu->space(AS_PROGRAM);

	// Call CPU-specific initialization
	init_cpu_for_idle<CPU_TYPE, LOAD_ADDR>(space);
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
std::pair<std::error_condition, std::string>
sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::quickload_cb(
    snapshot_image_device &image) {
	uint32_t size = image.length();
	uint32_t vram_addr = get_vram_addr();

	if (size > vram_addr - LOAD_ADDR)
		return std::make_pair(image_error::INVALIDLENGTH, "Program too large");

	std::vector<uint8_t> program(size);
	if (image.fread(&program[0], size) != size)
		return std::make_pair(image_error::UNSPECIFIED,
		                      "Failed to read program file");

	address_space &space = m_maincpu->space(AS_PROGRAM);
	for (uint32_t i = 0; i < size; i++)
		space.write_byte(LOAD_ADDR + i, program[i]);

	// Set reset vector for CPUs that use them
	uint32_t ram_size = get_ram_size();
	if (ram_size >= 0x10000) {
		space.write_byte(0xfffc, LOAD_ADDR & 0xff);
		space.write_byte(0xfffd, (LOAD_ADDR >> 8) & 0xff);
	}

	init_screen();

	return std::make_pair(std::error_condition(), std::string());
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

// Macro to define an SBC variant
// Parameters:
//   cpu_class: CPU device class (e.g., m6502_device)
//   cpu_type: CPU device type macro (e.g., M6502)
//   short_name: Short name for machine (e.g., 6502)
//   display_name: Human-readable CPU name (e.g., "MOS 6502")
//   ...: Optional overrides (load_addr, cpu_speed, vram_addr)
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

// Define SBC variants for major CPU architectures
DEFINE_SBC(m6502_device, M6502, 6502, "MOS 6502")
DEFINE_SBC(z80_device, Z80, z80, "Z80")
DEFINE_SBC(m68000_device, M68000, 68000, "Motorola 68000")
DEFINE_SBC(arm7_cpu_device, ARM7, arm7, "ARM7")
DEFINE_SBC(i386_device, I386, i386, "Intel 80386")
DEFINE_SBC(r3000a_device, R3000A, mips, "MIPS R3000A")
