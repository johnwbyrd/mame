// license:BSD-3-Clause
// copyright-holders:John Byrd
/***************************************************************************

    semihost.cpp

    ZBC Semihosting Device - provides file I/O, console, and time services
    to guest programs via memory-mapped RIFF-based protocol.

***************************************************************************/

#include "emu.h"
#include "emuopts.h"

#include "semihost.h"

// Logging configuration
#define LOG_REG (1U << 1)
#define LOG_REQUEST (1U << 2)
#define VERBOSE 0
#define LOG_OUTPUT_FUNC osd_printf_verbose
#include "logmacro.h"

#define LOGREG(...) LOGMASKED(LOG_REG, __VA_ARGS__)
#define LOGREQUEST(...) LOGMASKED(LOG_REQUEST, __VA_ARGS__)

//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(SEMIHOST, semihost_device, "semihost",
                   "ZBC Semihosting Device")

//**************************************************************************
//  DEVICE IMPLEMENTATION
//**************************************************************************

semihost_device::semihost_device(const machine_config &mconfig, const char *tag,
                                 device_t *owner, u32 clock)
    : device_t(mconfig, SEMIHOST, tag, owner, clock), m_irq_status(0),
      m_irq_enable(0), m_status(ZBC_STATUS_DEVICE_PRESENT), m_cpu(nullptr),
      m_irq_cb(*this) {}

void semihost_device::device_start() {
	// Resolve CPU device
	m_cpu = siblingdevice<cpu_device>(m_cpu_tag.c_str());
	if (!m_cpu)
		throw emu_fatalerror("semihost_device: CPU '%s' not found",
		                     m_cpu_tag.c_str());

	// Allocate work buffer
	m_work_buffer = std::make_unique<u8[]>(WORK_BUFFER_SIZE);

	// Initialize backend with MAME's share_directory as sandbox
	m_backend.reset(new zbc_ansi_state_t());
	zbc_ansi_init(m_backend.get(), machine().options().share_directory());
	zbc_ansi_set_callbacks(m_backend.get(), nullptr, on_exit_callback, this);

	// Initialize host with memory callbacks
	m_host = std::make_unique<zbc_host_state_t>();
	zbc_host_mem_ops_t mem_ops = {mem_read_u8, mem_write_u8, mem_read_block,
	                              mem_write_block};
	zbc_host_init(m_host.get(), &mem_ops, this, zbc_backend_ansi(),
	              m_backend.get(), m_work_buffer.get(), WORK_BUFFER_SIZE);

	LOG("semihost: initialized with sandbox '%s'\n",
	    machine().options().share_directory());

	// Save state
	save_item(NAME(m_riff_ptr));
	save_item(NAME(m_irq_status));
	save_item(NAME(m_irq_enable));
	save_item(NAME(m_status));
}

void semihost_device::device_reset() {
	std::memset(m_riff_ptr, 0, sizeof(m_riff_ptr));
	m_irq_status = 0;
	m_irq_enable = 0;
	m_status = ZBC_STATUS_DEVICE_PRESENT;

	if (!m_irq_cb.isunset())
		m_irq_cb(CLEAR_LINE);
}

void semihost_device::device_stop() {
	m_host.reset();
	m_backend.reset();
}

u8 semihost_device::read(offs_t offset) {
	static const char sig[] = ZBC_SIGNATURE_STR;

	switch (offset) {
	case 0x00 ... 0x07: // SIGNATURE
		LOGREG("semihost: read SIGNATURE[%d] = '%c'\n", offset, sig[offset]);
		return u8(sig[offset]);

	case 0x08 ... 0x17: // RIFF_PTR
		LOGREG("semihost: read RIFF_PTR[%d] = 0x%02x\n", offset - 0x08,
		       m_riff_ptr[offset - 0x08]);
		return m_riff_ptr[offset - 0x08];

	case ZBC_REG_DOORBELL: // Write-only
		return 0;

	case ZBC_REG_IRQ_STATUS:
		LOGREG("semihost: read IRQ_STATUS = 0x%02x\n", m_irq_status);
		return m_irq_status;

	case ZBC_REG_IRQ_ENABLE:
		LOGREG("semihost: read IRQ_ENABLE = 0x%02x\n", m_irq_enable);
		return m_irq_enable;

	case ZBC_REG_IRQ_ACK: // Write-only
		return 0;

	case ZBC_REG_STATUS:
		LOGREG("semihost: read STATUS = 0x%02x\n", m_status);
		return m_status;

	default:
		return (offset < ZBC_REG_SIZE) ? 0 : 0xff;
	}
}

void semihost_device::write(offs_t offset, u8 data) {
	switch (offset) {
	case 0x00 ... 0x07: // SIGNATURE - read-only
		break;

	case 0x08 ... 0x17: // RIFF_PTR
		m_riff_ptr[offset - 0x08] = data;
		LOGREG("semihost: write RIFF_PTR[%d] = 0x%02x\n", offset - 0x08, data);
		break;

	case ZBC_REG_DOORBELL:
		LOGREG("semihost: DOORBELL triggered\n");
		process_request();
		break;

	case ZBC_REG_IRQ_ENABLE:
		m_irq_enable = data;
		update_irq();
		break;

	case ZBC_REG_IRQ_ACK:
		m_irq_status &= ~data;
		update_irq();
		break;

	default: // STATUS and reserved are read-only
		break;
	}
}

void semihost_device::process_request() {
	m_status &= ~ZBC_STATUS_RESPONSE_READY;

	// Extract RIFF address from pointer bytes using CPU's endianness
	address_space &space = m_cpu->space(AS_PROGRAM);
	int addr_bytes = std::min((space.addr_width() + 7) / 8, 8);
	bool little_endian = (space.endianness() == ENDIANNESS_LITTLE);

	u64 riff_addr = 0;
	for (int i = 0; i < addr_bytes; i++) {
		int shift = little_endian ? (i * 8) : ((addr_bytes - 1 - i) * 8);
		riff_addr |= u64(m_riff_ptr[i]) << shift;
	}

	LOGREQUEST("semihost: process riff_addr=0x%llx\n",
	           (unsigned long long)riff_addr);

	int result = zbc_host_process(m_host.get(), riff_addr);

	m_status |= ZBC_STATUS_RESPONSE_READY;
	m_irq_status |= (result == 0) ? ZBC_IRQ_RESPONSE_READY : ZBC_IRQ_ERROR;
	update_irq();
}

void semihost_device::update_irq() {
	if (!m_irq_cb.isunset())
		m_irq_cb((m_irq_status & m_irq_enable) ? ASSERT_LINE : CLEAR_LINE);
}

//**************************************************************************
//  MEMORY ACCESS CALLBACKS
//**************************************************************************

u8 semihost_device::mem_read_u8(u64 addr, void *ctx) {
	return static_cast<semihost_device *>(ctx)
	    ->m_cpu->space(AS_PROGRAM)
	    .read_byte(addr);
}

void semihost_device::mem_write_u8(u64 addr, u8 val, void *ctx) {
	static_cast<semihost_device *>(ctx)
	    ->m_cpu->space(AS_PROGRAM)
	    .write_byte(addr, val);
}

void semihost_device::mem_read_block(void *dest, u64 addr, size_t size,
                                     void *ctx) {
	address_space &space =
	    static_cast<semihost_device *>(ctx)->m_cpu->space(AS_PROGRAM);
	u8 *dst = static_cast<u8 *>(dest);
	for (size_t i = 0; i < size; i++)
		dst[i] = space.read_byte(addr + i);
}

void semihost_device::mem_write_block(u64 addr, const void *src, size_t size,
                                      void *ctx) {
	address_space &space =
	    static_cast<semihost_device *>(ctx)->m_cpu->space(AS_PROGRAM);
	const u8 *s = static_cast<const u8 *>(src);
	for (size_t i = 0; i < size; i++)
		space.write_byte(addr + i, s[i]);
}

void semihost_device::on_exit_callback(void *ctx, unsigned int reason,
                                       unsigned int subcode) {
	auto *dev = static_cast<semihost_device *>(ctx);
	LOG("semihost: guest exit(%u, %u)\n", reason, subcode);
	dev->machine().schedule_exit();
}
