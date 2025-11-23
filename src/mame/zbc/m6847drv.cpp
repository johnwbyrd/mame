// license:BSD-3-Clause
// copyright-holders:John Byrd
/***************************************************************************

    MC6847 VDG Console Driver Implementation

***************************************************************************/

#include "m6847drv.h"
#include <ctype.h>
#include <string.h>
#include <assert.h>

MC6847Console::MC6847Console()
	: m_vram(nullptr)
	, m_cursor_pos(0)
{
}

void MC6847Console::set_vram_base(volatile uint8_t *vram)
{
	assert(vram != nullptr);
	m_vram = vram;
}

void MC6847Console::output_char(char c)
{
	assert(m_vram != nullptr);

	if (c == '\r' || c == '\n') {
		// Move to start of next line
		m_cursor_pos = ((m_cursor_pos / COLS) + 1) * COLS;
		if (m_cursor_pos >= VRAM_SIZE) {
			scroll();
			m_cursor_pos = VRAM_SIZE - COLS;
		}
	} else {
		// Output character (MC6847 only supports uppercase)
		m_vram[m_cursor_pos] = toupper(c);
		m_cursor_pos++;
		if (m_cursor_pos >= VRAM_SIZE) {
			scroll();
			m_cursor_pos = VRAM_SIZE - COLS;
		}
	}
}

void MC6847Console::output_string(const char *str)
{
	assert(m_vram != nullptr);
	assert(str != nullptr);

	while (*str) {
		output_char(*str++);
	}
}

void MC6847Console::clear_screen()
{
	assert(m_vram != nullptr);

	for (int i = 0; i < VRAM_SIZE; i++) {
		m_vram[i] = 0x20; // Space character
	}
	m_cursor_pos = 0;
}

void MC6847Console::goto_xy(uint8_t col, uint8_t row)
{
	assert(m_vram != nullptr);

	// Clamp to valid range
	if (col >= COLS) col = COLS - 1;
	if (row >= ROWS) row = ROWS - 1;

	m_cursor_pos = row * COLS + col;
}

void MC6847Console::scroll()
{
	assert(m_vram != nullptr);

	// Copy all lines up by one
	for (int i = 0; i < VRAM_SIZE - COLS; i++) {
		m_vram[i] = m_vram[i + COLS];
	}

	// Clear bottom line
	for (int i = VRAM_SIZE - COLS; i < VRAM_SIZE; i++) {
		m_vram[i] = 0x20;
	}

	// Adjust cursor
	m_cursor_pos -= COLS;
	if (m_cursor_pos < 0) {
		m_cursor_pos = 0;
	}
}

void MC6847Console::print_word(const char *word)
{
	assert(m_vram != nullptr);
	assert(word != nullptr);

	int word_len = strlen(word);
	int col = m_cursor_pos % COLS;

	int needed = word_len;
	if (col > 0) {
		needed++; // Space before word
	}

	// Word wrap: if word won't fit on current line, move to next line
	if (col > 0 && col + needed > COLS) {
		output_char('\r');
		col = 0;
	}

	// Add space separator if not at start of line
	if (col > 0) {
		output_char(' ');
	}

	// Output word
	for (int i = 0; i < word_len; i++) {
		output_char(word[i]);
	}
}

void MC6847Console::print_sentence(const char *text)
{
	assert(m_vram != nullptr);
	assert(text != nullptr);

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
			// Accumulate word
			if (word_idx < 63) {
				word[word_idx++] = *p;
			}
		}
	}

	// Output final word if any
	if (word_idx > 0) {
		word[word_idx] = '\0';
		print_word(word);
	}
}

void MC6847Console::center_line(const char *text)
{
	assert(m_vram != nullptr);
	assert(text != nullptr);

	int len = strlen(text);
	int padding = (COLS - len) / 2;

	// Add leading spaces
	for (int i = 0; i < padding; i++) {
		output_char(' ');
	}

	// Output text
	for (int i = 0; i < len; i++) {
		output_char(text[i]);
	}

	// Add newline if not already at start of line
	if ((m_cursor_pos % COLS) != 0) {
		output_char('\r');
	}
}
