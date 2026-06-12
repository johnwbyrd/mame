// license:BSD-3-Clause
// copyright-holders:John Byrd
/***************************************************************************

    semihost.h

    ZBC Semihosting Device - provides file I/O, console, time services,
    and timer interrupts to guest programs via memory-mapped RIFF-based
    protocol.

    Files are sandboxed to MAME's share_directory (configurable via
    -share_directory).

***************************************************************************/

#ifndef MAME_MACHINE_SEMIHOST_H
#define MAME_MACHINE_SEMIHOST_H

#pragma once

extern "C" {
#include "semihost/include/zbc_backend.h"
#include "semihost/include/zbc_backend_ansi.h"
#include "semihost/include/zbc_host.h"
#include "semihost/include/zbc_protocol.h"
}

class semihost_device : public device_t {
  public:
	semihost_device(const machine_config &mconfig, const char *tag,
	                device_t *owner, u32 clock = 0);
	~semihost_device() = default;

	// Configuration
	void set_cpu_tag(const char *tag) { m_cpu_tag = tag; }

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

	// Custom deleters for RAII
	struct backend_deleter {
		void operator()(zbc_ansi_state_t *p) const {
			if (p) {
				zbc_ansi_cleanup(p);
				delete p;
			}
		}
	};

	// C library state (RAII managed)
	std::unique_ptr<zbc_ansi_state_t, backend_deleter> m_backend;
	std::unique_ptr<zbc_host_state_t> m_host;
	std::unique_ptr<u8[]> m_work_buffer;

	// Device registers
	u8 m_riff_ptr[16]; // Supports up to 128-bit guest pointers
	u8 m_status;       // Interrupt pending indicator (0 = none, 1 = timer, etc.)

	// Timer state
	emu_timer *m_timer;     // Periodic timer for interrupts
	u32 m_timer_rate;       // Current rate in Hz (0 = disabled)

	// Configuration and references
	std::string m_cpu_tag;
	cpu_device *m_cpu;
	devcb_write_line m_irq_cb;

	// Internal methods
	void process_request();
	int handle_timer_config(u32 rate_hz);
	TIMER_CALLBACK_MEMBER(timer_tick);

	// Static callbacks for C library
	static u8 mem_read_u8(uintptr_t addr, void *ctx);
	static void mem_write_u8(uintptr_t addr, u8 val, void *ctx);
	static void mem_read_block(void *dest, uintptr_t addr, size_t size, void *ctx);
	static void mem_write_block(uintptr_t addr, const void *src, size_t size,
	                            void *ctx);
	static void on_exit_callback(void *ctx, unsigned int reason,
	                             unsigned int subcode);
	static int on_timer_config_callback(void *ctx, unsigned int rate_hz);
};

DECLARE_DEVICE_TYPE(SEMIHOST, semihost_device)

#endif // MAME_MACHINE_SEMIHOST_H
