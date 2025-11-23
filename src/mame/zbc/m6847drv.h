// license:BSD-3-Clause
// copyright-holders:John Byrd
/***************************************************************************

    MC6847 VDG Console Driver

    Minimal C++ class for MC6847 text mode console I/O (32x16 characters).
    Designed for embedded use with no C++ standard library dependencies.

    Usage:
        MC6847Console console;
        console.set_vram_base(vram_pointer);
        console.clear_screen();
        console.output_string("Hello, world!\n");

***************************************************************************/

#ifndef MAME_ZBC_M6847DRV_H
#define MAME_ZBC_M6847DRV_H

#pragma once

#include <stdint.h>
#include <stddef.h>

class MC6847Console {
public:
	MC6847Console();

	// Runtime configuration - MUST be called before any output operations
	void set_vram_base(volatile uint8_t *vram);

	// Core console I/O functions
	void output_char(char c);
	void output_string(const char *str);
	void clear_screen();
	void goto_xy(uint8_t col, uint8_t row);
	void scroll();

	// Query functions
	uint16_t get_cursor_pos() const { return m_cursor_pos; }

	// High-level text formatting functions
	void print_word(const char *word);
	void print_sentence(const char *text);
	void center_line(const char *text);

private:
	// MC6847 VDG text mode: 32 columns × 16 rows = 512 bytes
	static constexpr int COLS = 32;
	static constexpr int ROWS = 16;
	static constexpr int VRAM_SIZE = 512;

	volatile uint8_t *m_vram;
	uint16_t m_cursor_pos;
};

#endif // MAME_ZBC_M6847DRV_H
