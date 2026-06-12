// license:BSD-3-Clause
// copyright-holders:John Byrd
/*
 * semihost.cpp - ZBC Semihosting Device
 *
 * Provides file I/O, console, time services, and timer interrupts to guest
 * programs via memory-mapped RIFF-based protocol.
 */

#include "emu.h"
#include "emuopts.h"
#include "semihost.h"

#define LOG_REG     (1U << 1)
#define LOG_REQUEST (1U << 2)
#define LOG_TIMER   (1U << 3)
#define VERBOSE     (LOG_REG | LOG_REQUEST | LOG_TIMER)
#define LOG_OUTPUT_FUNC osd_printf_verbose
#include "logmacro.h"

#define LOGREG(...)     LOGMASKED(LOG_REG, __VA_ARGS__)
#define LOGREQUEST(...) LOGMASKED(LOG_REQUEST, __VA_ARGS__)
#define LOGTIMER(...)   LOGMASKED(LOG_TIMER, __VA_ARGS__)

DEFINE_DEVICE_TYPE(SEMIHOST, semihost_device, "semihost", "ZBC Semihosting Device")

semihost_device::semihost_device(const machine_config &mconfig, const char *tag,
                                 device_t *owner, u32 clock)
    : device_t(mconfig, SEMIHOST, tag, owner, clock),
      m_status(ZBC_STATUS_NONE),
      m_timer(nullptr),
      m_timer_rate(0),
      m_cpu(nullptr),
      m_irq_cb(*this)
{
}

void semihost_device::device_start()
{
	m_cpu = siblingdevice<cpu_device>(m_cpu_tag.c_str());
	if (!m_cpu)
		throw emu_fatalerror("semihost_device: CPU '%s' not found", m_cpu_tag.c_str());

	m_work_buffer = std::make_unique<u8[]>(WORK_BUFFER_SIZE);

	m_backend.reset(new zbc_ansi_state_t());
	zbc_ansi_init(m_backend.get(), machine().options().share_directory());
	zbc_ansi_set_callbacks(m_backend.get(), nullptr, on_exit_callback,
	                       on_timer_config_callback, this);

	m_host = std::make_unique<zbc_host_state_t>();
	zbc_host_mem_ops_t mem_ops = {mem_read_u8, mem_write_u8, mem_read_block, mem_write_block};
	zbc_host_init(m_host.get(), &mem_ops, this, zbc_backend_ansi(),
	              m_backend.get(), m_work_buffer.get(), WORK_BUFFER_SIZE);

	LOG("semihost: initialized with sandbox '%s'\n", machine().options().share_directory());

	m_timer = timer_alloc(FUNC(semihost_device::timer_tick), this);

	save_item(NAME(m_riff_ptr));
	save_item(NAME(m_status));
	save_item(NAME(m_timer_rate));
}

void semihost_device::device_reset()
{
	std::memset(m_riff_ptr, 0, sizeof(m_riff_ptr));
	m_status = ZBC_STATUS_NONE;
	m_timer_rate = 0;

	if (m_timer)
		m_timer->enable(false);

	if (!m_irq_cb.isunset())
		m_irq_cb(CLEAR_LINE);
}

void semihost_device::device_stop()
{
	m_host.reset();
	m_backend.reset();
}

u8 semihost_device::read(offs_t offset)
{
	static const char sig[] = ZBC_SIGNATURE_STR;

	if (offset < ZBC_REG_RIFF_PTR) {
		LOGREG("semihost: read SIGNATURE[%d] = '%c'\n", offset, sig[offset]);
		return u8(sig[offset]);
	}

	if (offset < ZBC_REG_DOORBELL) {
		int idx = offset - ZBC_REG_RIFF_PTR;
		LOGREG("semihost: read RIFF_PTR[%d] = 0x%02x\n", idx, m_riff_ptr[idx]);
		return m_riff_ptr[idx];
	}

	if (offset == ZBC_REG_DOORBELL)
		return 0;

	if (offset == ZBC_REG_STATUS) {
		LOGREG("semihost: read STATUS = 0x%02x\n", m_status);
		return m_status;
	}

	return (offset < ZBC_REG_SIZE) ? 0 : 0xff;
}

void semihost_device::write(offs_t offset, u8 data)
{
	if (offset < ZBC_REG_RIFF_PTR)
		return;

	if (offset < ZBC_REG_DOORBELL) {
		int idx = offset - ZBC_REG_RIFF_PTR;
		m_riff_ptr[idx] = data;
		LOGREG("semihost: write RIFF_PTR[%d] = 0x%02x\n", idx, data);
		return;
	}

	if (offset == ZBC_REG_DOORBELL) {
		LOGREG("semihost: DOORBELL triggered\n");
		process_request();
		return;
	}

	if (offset == ZBC_REG_STATUS) {
		if (data == 0) {
			LOGREG("semihost: STATUS cleared, deasserting IRQ\n");
			m_status = ZBC_STATUS_NONE;
			if (!m_irq_cb.isunset())
				m_irq_cb(CLEAR_LINE);
		}
		return;
	}
}

void semihost_device::process_request()
{
	address_space &space = m_cpu->space(AS_PROGRAM);
	int addr_bytes = std::min((space.addr_width() + 7) / 8, 8);
	bool little_endian = (space.endianness() == ENDIANNESS_LITTLE);

	u64 riff_addr = 0;
	for (int i = 0; i < addr_bytes; i++) {
		int shift = little_endian ? (i * 8) : ((addr_bytes - 1 - i) * 8);
		riff_addr |= u64(m_riff_ptr[i]) << shift;
	}

	LOGREQUEST("semihost: riff_addr=0x%llx\n", (unsigned long long)riff_addr);
	int result = zbc_host_process(m_host.get(), riff_addr);
	LOGREQUEST("semihost: result=%d\n", result);
}

int semihost_device::handle_timer_config(u32 rate_hz)
{
	LOGTIMER("semihost: timer_config rate=%u Hz\n", rate_hz);

	if (rate_hz == 0) {
		m_timer->enable(false);
		m_timer_rate = 0;

		if (m_status == ZBC_STATUS_TIMER) {
			m_status = ZBC_STATUS_NONE;
			if (!m_irq_cb.isunset())
				m_irq_cb(CLEAR_LINE);
		}

		LOGTIMER("semihost: timer disabled\n");
		return ZBC_OK;
	}

	if (rate_hz > 10000000) {
		LOGTIMER("semihost: timer rate %u Hz too high\n", rate_hz);
		return ZBC_ERR_INVALID_ARG;
	}

	attotime period = attotime::from_hz(rate_hz);
	m_timer->adjust(period, 0, period);
	m_timer_rate = rate_hz;

	LOGTIMER("semihost: timer enabled at %u Hz (period=%s)\n", rate_hz, period.as_string());
	return ZBC_OK;
}

TIMER_CALLBACK_MEMBER(semihost_device::timer_tick)
{
	m_status = ZBC_STATUS_TIMER;

	if (!m_irq_cb.isunset())
		m_irq_cb(ASSERT_LINE);

	LOGTIMER("semihost: timer tick, STATUS=%d, asserting IRQ\n", m_status);
}

u8 semihost_device::mem_read_u8(uintptr_t addr, void *ctx)
{
	return static_cast<semihost_device *>(ctx)->m_cpu->space(AS_PROGRAM).read_byte(addr);
}

void semihost_device::mem_write_u8(uintptr_t addr, u8 val, void *ctx)
{
	static_cast<semihost_device *>(ctx)->m_cpu->space(AS_PROGRAM).write_byte(addr, val);
}

void semihost_device::mem_read_block(void *dest, uintptr_t addr, size_t size, void *ctx)
{
	address_space &space = static_cast<semihost_device *>(ctx)->m_cpu->space(AS_PROGRAM);
	u8 *dst = static_cast<u8 *>(dest);
	for (size_t i = 0; i < size; i++)
		dst[i] = space.read_byte(addr + i);
}

void semihost_device::mem_write_block(uintptr_t addr, const void *src, size_t size, void *ctx)
{
	address_space &space = static_cast<semihost_device *>(ctx)->m_cpu->space(AS_PROGRAM);
	const u8 *s = static_cast<const u8 *>(src);
	for (size_t i = 0; i < size; i++)
		space.write_byte(addr + i, s[i]);
}

void semihost_device::on_exit_callback(void *ctx, unsigned int reason, unsigned int subcode)
{
	auto *dev = static_cast<semihost_device *>(ctx);
	LOG("semihost: guest exit(%u, %u)\n", reason, subcode);
	dev->machine().schedule_exit();
}

int semihost_device::on_timer_config_callback(void *ctx, unsigned int rate_hz)
{
	return static_cast<semihost_device *>(ctx)->handle_timer_config(rate_hz);
}
