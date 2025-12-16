// license:BSD-3-Clause
// copyright-holders:John Byrd
/*********************************************************************

    elfload.h

    ELF executable loader for emulated systems

    Loads PT_LOAD segments from static ELF executables into guest
    memory, then resets the CPU. No relocation support - ELF must
    be fully resolved (ET_EXEC with no relocations).

*********************************************************************/

#ifndef MAME_IMAGEDEV_ELFLOAD_H
#define MAME_IMAGEDEV_ELFLOAD_H

#pragma once

#include "snapquik.h"

class elfload_image_device : public snapshot_image_device
{
public:
	elfload_image_device(const machine_config &mconfig, const char *tag,
	                     device_t *owner, uint32_t clock = 0U);

	// Configure target CPU (required)
	template <typename T> void set_cpu(T &&tag) { m_cpu.set_tag(std::forward<T>(tag)); }

	virtual const char *file_extensions() const noexcept override { return "elf"; }
	virtual const char *image_type_name() const noexcept override { return "elfload"; }
	virtual const char *image_brief_type_name() const noexcept override { return "elf"; }

protected:
	virtual void device_start() override ATTR_COLD;

private:
	required_device<cpu_device> m_cpu;

	std::pair<std::error_condition, std::string> load_elf(snapshot_image_device &image);

	// ELF parsing - returns false on error
	bool parse_elf32(const uint8_t *data, size_t size);
	bool parse_elf64(const uint8_t *data, size_t size);

	// Load a single PT_LOAD segment into guest memory
	void load_segment(uint64_t vaddr, const uint8_t *data, size_t filesz, size_t memsz);

	// Report ELF type error with helpful message
	void report_elf_type_error(uint16_t e_type);
};

DECLARE_DEVICE_TYPE(ELFLOAD, elfload_image_device)

#endif // MAME_IMAGEDEV_ELFLOAD_H
