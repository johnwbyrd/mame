// license:BSD-3-Clause
// copyright-holders:John Byrd
/***************************************************************************

    zbc.h

    Zero Board Computer (ZBC) semihosting device. Provides file I/O,
    console, time services, and timer interrupts to guest programs via a
    32-byte memory-mapped RIFF-based protocol.

    Files are sandboxed to MAME's share_directory (configurable via
    -share_directory).

***************************************************************************/

#ifndef MAME_MACHINE_ZBC_H
#define MAME_MACHINE_ZBC_H

#pragma once

#include "zbc/Semihost.h"

class zbc_device : public device_t {
  public:
	zbc_device(const machine_config &mconfig, const char *tag,
	           device_t *owner, u32 clock = 0);
	~zbc_device();

	void set_cpu_tag(const char *tag) { m_cpu_tag = tag; }

	u8 read(offs_t offset);
	void write(offs_t offset, u8 data);

	auto irq_callback() { return m_irq_cb.bind(); }

  protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_stop() override ATTR_COLD;

  private:
	bool on_timer_config(unsigned rate_hz);
	TIMER_CALLBACK_MEMBER(timer_tick);
	void rebuild();

	std::string m_cpu_tag;
	cpu_device *m_cpu;
	devcb_write_line m_irq_cb;
	emu_timer *m_timer;
	u32 m_timer_rate;

	std::unique_ptr<zbc::GuestMemory> m_guest_mem;
	std::unique_ptr<zbc::Device> m_zbc;
};

DECLARE_DEVICE_TYPE(ZBC, zbc_device)

#endif // MAME_MACHINE_ZBC_H
