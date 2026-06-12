// license:BSD-3-Clause
// copyright-holders:John Byrd
/*
 * zbc.cpp - Zero Board Computer semihosting device
 *
 * Memory-mapped 32-byte device providing file I/O, console, time
 * services, and timer interrupts via the ZBC RIFF protocol. Wraps the
 * upstream C++ host library (3rdparty/zbc/include/cpp/zbc).
 */

#include "emu.h"
#include "emuopts.h"
#include "zbc.h"

#define LOG_GENERAL (1U << 0)
#define LOG_TIMER   (1U << 1)
#define VERBOSE     (LOG_GENERAL | LOG_TIMER)
#define LOG_OUTPUT_FUNC osd_printf_verbose
#include "logmacro.h"

#define LOGTIMER(...) LOGMASKED(LOG_TIMER, __VA_ARGS__)

DEFINE_DEVICE_TYPE(ZBC, zbc_device, "zbc", "Zero Board Computer Semihosting Device")

namespace {

class mame_guest_memory : public zbc::GuestMemory {
  public:
	explicit mame_guest_memory(address_space &space) : m_space(space) {}

	uint8_t readByte(uint64_t addr) override { return m_space.read_byte(addr); }
	void writeByte(uint64_t addr, uint8_t value) override { m_space.write_byte(addr, value); }

  private:
	address_space &m_space;
};

uint8_t pick_size_bytes(int addr_bits) {
	int bytes = (addr_bits + 7) / 8;
	if (bytes <= 1) return 1;
	if (bytes <= 2) return 2;
	if (bytes <= 4) return 4;
	return 8;
}

} // anonymous namespace

zbc_device::zbc_device(const machine_config &mconfig, const char *tag,
                       device_t *owner, u32 clock)
    : device_t(mconfig, ZBC, tag, owner, clock),
      m_cpu(nullptr),
      m_irq_cb(*this),
      m_timer(nullptr),
      m_timer_rate(0)
{
}

zbc_device::~zbc_device() = default;

void zbc_device::device_start()
{
	m_cpu = siblingdevice<cpu_device>(m_cpu_tag.c_str());
	if (!m_cpu)
		throw emu_fatalerror("zbc_device: CPU '%s' not found", m_cpu_tag.c_str());

	m_timer = timer_alloc(FUNC(zbc_device::timer_tick), this);
	rebuild();

	LOG("zbc: initialized with sandbox '%s'\n", machine().options().share_directory());

	save_item(NAME(m_timer_rate));
}

void zbc_device::device_reset()
{
	if (m_timer)
		m_timer->enable(false);
	m_timer_rate = 0;

	rebuild();

	if (!m_irq_cb.isunset())
		m_irq_cb(CLEAR_LINE);
}

void zbc_device::device_stop()
{
	m_zbc.reset();
	m_guest_mem.reset();
}

void zbc_device::rebuild()
{
	address_space &space = m_cpu->space(AS_PROGRAM);

	uint8_t ptr_size = pick_size_bytes(space.addr_width());
	zbc::Endian endian = (space.endianness() == ENDIANNESS_LITTLE)
	                         ? zbc::Endian::Little
	                         : zbc::Endian::Big;
	zbc::PlatformConfig config(ptr_size, ptr_size, endian);

	m_guest_mem = std::make_unique<mame_guest_memory>(space);

	auto on_exit = [this](unsigned reason, unsigned subcode) {
		LOG("zbc: guest exit(%u, %u)\n", reason, subcode);
		machine().schedule_exit();
	};
	auto on_timer = [this](unsigned rate_hz) {
		return on_timer_config(rate_hz);
	};

	m_zbc = std::make_unique<zbc::Device>(
	    *m_guest_mem, config,
	    std::make_unique<zbc::FileBackend>(on_exit, on_timer),
	    std::make_unique<zbc::SandboxedPolicy>(machine().options().share_directory()));

	m_zbc->setIrqCallback([this](bool assert) {
		if (!m_irq_cb.isunset())
			m_irq_cb(assert ? ASSERT_LINE : CLEAR_LINE);
	});
}

u8 zbc_device::read(offs_t offset)
{
	return m_zbc->read(offset);
}

void zbc_device::write(offs_t offset, u8 data)
{
	m_zbc->write(offset, data);
}

bool zbc_device::on_timer_config(unsigned rate_hz)
{
	LOGTIMER("zbc: timer_config rate=%u Hz\n", rate_hz);

	if (rate_hz == 0) {
		m_timer->enable(false);
		m_timer_rate = 0;
		LOGTIMER("zbc: timer disabled\n");
		return true;
	}

	if (rate_hz > 10000000) {
		LOGTIMER("zbc: timer rate %u Hz too high\n", rate_hz);
		return false;
	}

	attotime period = attotime::from_hz(rate_hz);
	m_timer->adjust(period, 0, period);
	m_timer_rate = rate_hz;

	LOGTIMER("zbc: timer enabled at %u Hz (period=%s)\n", rate_hz, period.as_string());
	return true;
}

TIMER_CALLBACK_MEMBER(zbc_device::timer_tick)
{
	m_zbc->timerTick();
	LOGTIMER("zbc: timer tick\n");
}
