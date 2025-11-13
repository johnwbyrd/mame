// license:BSD-3-Clause
// copyright-holders:MAME Team
/***************************************************************************

    Single Board Computer MOS 6502

    Minimal M6502 system for testing and development
    - 64KB RAM (no ROM)
    - MC6847 VDG text display
    - Quickload support for loading programs

    Memory Map:
    0x0000-0xFCFF: RAM (52,224 bytes)
    0xFD00-0xFEFF: Video RAM (512 bytes, 32x16 text display)
    0xFF00-0xFFFF: RAM (256 bytes, includes reset vectors at 0xFFFC-0xFFFF)

    Usage:
    mame sbc6502 -quik program.bin

	The program will be loaded at address 0x0200 and executed.

****************************************************************************/

#include "cpu/m6502/m6502.h"
#include "emu.h"
#include "emupal.h"
#include "imagedev/snapquik.h"
#include "screen.h"
#include "video/mc6847.h"

namespace {

class sbc6502_state : public driver_device {
  public:
	sbc6502_state(const machine_config &mconfig, device_type type,
	              const char *tag)
	    : driver_device(mconfig, type, tag), m_maincpu(*this, "maincpu"),
	      m_vdg(*this, "vdg"), m_videoram(*this, "videoram") {}

	void sbc6502(machine_config &config) ATTR_COLD;

  protected:
	virtual void machine_reset() override ATTR_COLD;

  private:
	required_device<cpu_device> m_maincpu;
	required_device<mc6847_base_device> m_vdg;
	required_shared_ptr<uint8_t> m_videoram;

	void mem_map(address_map &map) ATTR_COLD;
	void init_screen();
	void chrout(char c);
	void print_word(const char *word);
	void print_sentence(const char *text);
	void center_line(const char *text);
	void scroll_up();

	DECLARE_QUICKLOAD_LOAD_MEMBER(quickload_cb);

	uint8_t vdg_videoram_r(offs_t offset);

	int m_cursor_pos;
	static constexpr int LINE_WIDTH = 32;
	static constexpr int SCREEN_HEIGHT = 16;
	static constexpr int SCREEN_SIZE = 0x200;
};

void sbc6502_state::mem_map(address_map &map) {
	map.unmap_value_high();
	map(0x0000, 0xfcff).ram();                   // General RAM
	map(0xfd00, 0xfeff).ram().share("videoram"); // Video RAM (512 bytes)
	map(0xff00, 0xffff).ram(); // High RAM (includes reset vectors)
}

uint8_t sbc6502_state::vdg_videoram_r(offs_t offset) {
	// MC6847 reads from video RAM for display
	// Maps to 0xFD00-0xFEFF (512 bytes for 32x16 text mode)
	return m_videoram[offset & 0x1ff];
}

void sbc6502_state::scroll_up() {
	// Move all lines up by one
	for (int i = 0; i < SCREEN_SIZE - LINE_WIDTH; i++)
		m_videoram[i] = m_videoram[i + LINE_WIDTH];

	// Clear bottom line
	for (int i = SCREEN_SIZE - LINE_WIDTH; i < SCREEN_SIZE; i++)
		m_videoram[i] = 0x20;

	// Move cursor back by one line
	m_cursor_pos -= LINE_WIDTH;
	if (m_cursor_pos < 0)
		m_cursor_pos = 0;
}

void sbc6502_state::chrout(char c) {
	if (c == '\r' || c == '\n') {
		// Move to start of next line
		m_cursor_pos = ((m_cursor_pos / LINE_WIDTH) + 1) * LINE_WIDTH;
		if (m_cursor_pos >= SCREEN_SIZE) {
			scroll_up();
			m_cursor_pos = SCREEN_SIZE - LINE_WIDTH;
		}
	} else {
		// Write character at current cursor position
		m_videoram[m_cursor_pos] = toupper(c);
		m_cursor_pos++;
		if (m_cursor_pos >= SCREEN_SIZE) {
			scroll_up();
			m_cursor_pos = SCREEN_SIZE - LINE_WIDTH;
		}
	}
}

void sbc6502_state::print_word(const char *word) {
	int word_len = strlen(word);
	int col = m_cursor_pos % LINE_WIDTH;

	// Check if word fits on current line (with space before if not at line start)
	int needed = word_len;
	if (col > 0)
		needed++; // Need space before word

	if (col > 0 && col + needed > LINE_WIDTH) {
		// Word won't fit, move to next line
		chrout('\r');
		col = 0;
	}

	// Add space before word if not at start of line
	if (col > 0)
		chrout(' ');

	// Print the word
	for (int i = 0; i < word_len; i++)
		chrout(word[i]);
}

void sbc6502_state::print_sentence(const char *text) {
	char word[64];
	int word_idx = 0;

	for (const char *p = text; *p; p++) {
		if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
			// End of word
			if (word_idx > 0) {
				word[word_idx] = '\0';
				print_word(word);
				word_idx = 0;
			}
		} else {
			// Build word
			if (word_idx < 63)
				word[word_idx++] = *p;
		}
	}

	// Print last word if any
	if (word_idx > 0) {
		word[word_idx] = '\0';
		print_word(word);
	}
}

void sbc6502_state::center_line(const char *text) {
	int len = strlen(text);
	int padding = (LINE_WIDTH - len) / 2;

	for (int i = 0; i < padding; i++)
		chrout(' ');

	for (int i = 0; i < len; i++)
		chrout(text[i]);

	chrout('\r');
}

void sbc6502_state::init_screen() {
	// Clear screen with spaces
	for (int i = 0; i < 0x200; i++)
		m_videoram[i] = 0x20;

	m_cursor_pos = 0;

	center_line("Single board computer");
	center_line("MOS 6502");
	center_line("");
	center_line("github.com/johnwbyrd/semihost");
	center_line("");

	print_sentence("This is a minimal 6502 system with 64KB RAM and a "
					"MC6847 video display at $FD00. "
					"Load a headerless binary in MAME using the -quik option. "
					"The program will be loaded and executed at $0200. "
					"Happy coding!");
}

void sbc6502_state::machine_reset() {
	// Initialize screen with test pattern
	init_screen();

	// Set reset vector to 0x0200 (in high RAM at 0xFFFC-0xFFFF)
	address_space &space = m_maincpu->space(AS_PROGRAM);
	space.write_byte(0xfffc, 0x00); // Low byte
	space.write_byte(0xfffd, 0x02); // High byte - points to 0x0200
}

QUICKLOAD_LOAD_MEMBER(sbc6502_state::quickload_cb) {
	uint32_t size = image.length();

	// Limit program size to avoid overwriting video RAM
	if (size > 0xfd00)
		return std::make_pair(image_error::INVALIDLENGTH,
		                      "Program too large (max 64768 bytes)");

	// Load program at 0x0200 (after zero page and stack)
	std::vector<uint8_t> program(size);
	if (image.fread(&program[0], size) != size)
		return std::make_pair(image_error::UNSPECIFIED,
		                      "Failed to read program file");

	// Copy program to RAM
	address_space &space = m_maincpu->space(AS_PROGRAM);
	for (uint32_t i = 0; i < size; i++)
		space.write_byte(0x0200 + i, program[i]);

	// Set reset vector to 0x0200 (in high RAM at 0xFFFC-0xFFFF)
	// Note: 6502 reset vector at 0xFFFC-0xFFFD (little-endian)
	space.write_byte(0xfffc, 0x00); // Low byte of 0x0200
	space.write_byte(0xfffd, 0x02); // High byte of 0x0200

	// Re-initialize screen after loading
	init_screen();

	return std::make_pair(std::error_condition(), std::string());
}

void sbc6502_state::sbc6502(machine_config &config) {
	// CPU: M6502 at 10MHz
	M6502(config, m_maincpu, 10'000'000);
	m_maincpu->set_addrmap(AS_PROGRAM, &sbc6502_state::mem_map);

	// Video: MC6847 VDG
	SCREEN(config, "screen", SCREEN_TYPE_RASTER);

	MC6847(config, m_vdg, 4.433619_MHz_XTAL, true); // PAL mode
	m_vdg->set_screen("screen");
	m_vdg->fsync_wr_callback().set_inputline(m_maincpu, INPUT_LINE_NMI);
	m_vdg->input_callback().set(FUNC(sbc6502_state::vdg_videoram_r));
	// MC6847 uses internal character ROM by default for text mode

	// Quickload for loading programs
	QUICKLOAD(config, "quickload", "bin")
	    .set_load_callback(FUNC(sbc6502_state::quickload_cb));
}

ROM_START(sbc6502)
// No ROM - pure RAM system
ROM_END

} // anonymous namespace

//    YEAR  NAME      PARENT  COMPAT  MACHINE   INPUT  CLASS           INIT
//    COMPANY  FULLNAME                          FLAGS
COMP(2025, sbc6502, 0, 0, sbc6502, 0, sbc6502_state, empty_init, "MAME",
     "Single Board Computer - MOS 6502", MACHINE_NO_SOUND_HW)
