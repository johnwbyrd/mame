// license:BSD-3-Clause
// copyright-holders:John Byrd
/***************************************************************************

    semihost.h

    ZBC Semihosting Device

    A memory-mapped semihosting device that provides file I/O, console,
    and time services to guest programs via RIFF-based protocol.

***************************************************************************/

#ifndef MAME_MACHINE_SEMIHOST_H
#define MAME_MACHINE_SEMIHOST_H

#pragma once

// C semihosting library headers
extern "C" {
#include "semihost/include/zbc_host.h"
#include "semihost/include/zbc_backend.h"
#include "semihost/include/zbc_backend_ansi.h"
}

class semihost_device : public device_t
{
public:
	semihost_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);
	virtual ~semihost_device();

	// Configuration
	void set_cpu_tag(const char *tag) { m_cpu_tag = tag; }
	void set_sandbox_dir(const char *dir) { m_sandbox_dir = dir ? dir : ""; }

	// Memory-mapped register access (32 bytes)
	u8 read(offs_t offset);
	void write(offs_t offset, u8 data);

	// IRQ output callback
	auto irq_callback() { return m_irq_cb.bind(); }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

private:
	static constexpr size_t WORK_BUFFER_SIZE = 4096;

	// C library state
	zbc_host_state_t *m_host;
	zbc_ansi_state_t *m_backend;
	std::unique_ptr<u8[]> m_work_buffer;

	// Device registers
	u8 m_riff_ptr[16];  // 16 bytes to support up to 128-bit guest pointers
	u8 m_irq_status;
	u8 m_irq_enable;
	u8 m_status;

	// Configuration
	std::string m_cpu_tag;
	std::string m_sandbox_dir;

	// CPU reference for memory access
	cpu_device *m_cpu;

	// IRQ callback
	devcb_write_line m_irq_cb;

	// Process semihosting request (called on doorbell write)
	void process_request();

	// Update IRQ output based on status and enable
	void update_irq();

	// Memory access callbacks for C library
	static u8 mem_read_u8(u64 addr, void *ctx);
	static void mem_write_u8(u64 addr, u8 val, void *ctx);
	static void mem_read_block(void *dest, u64 addr, size_t size, void *ctx);
	static void mem_write_block(u64 addr, const void *src, size_t size, void *ctx);

	// Exit callback for C library (schedules MAME to exit)
	static void on_exit_callback(void *ctx, unsigned int reason, unsigned int subcode);

	// Get default sandbox directory (cross-platform)
	static std::string get_default_sandbox_dir();
};

DECLARE_DEVICE_TYPE(SEMIHOST, semihost_device)

#endif // MAME_MACHINE_SEMIHOST_H
