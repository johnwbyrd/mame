// license:BSD-3-Clause
// copyright-holders:John Byrd
/***************************************************************************

    semihost.cpp

    ZBC Semihosting Device

    A memory-mapped semihosting device that provides file I/O, console,
    and time services to guest programs via RIFF-based protocol.

***************************************************************************/

#include "emu.h"
#include "semihost.h"

#include "cpu/m6502/m6502.h"

// For cross-platform home directory
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define LOG_GENERAL (1U << 0)
#define LOG_REG     (1U << 1)
#define LOG_REQUEST (1U << 2)

#define VERBOSE (LOG_GENERAL | LOG_REG | LOG_REQUEST)
#include "logmacro.h"

#define LOGREG(...)     LOGMASKED(LOG_REG, __VA_ARGS__)
#define LOGREQUEST(...) LOGMASKED(LOG_REQUEST, __VA_ARGS__)


//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(SEMIHOST, semihost_device, "semihost", "ZBC Semihosting Device")


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  semihost_device - constructor
//-------------------------------------------------

semihost_device::semihost_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, SEMIHOST, tag, owner, clock)
	, m_host(nullptr)
	, m_backend(nullptr)
	, m_work_buffer(nullptr)
	, m_riff_ptr(0)
	, m_irq_status(0)
	, m_irq_enable(0)
	, m_status(ZBC_STATUS_DEVICE_PRESENT)
	, m_cpu_tag()
	, m_sandbox_dir()
	, m_cpu(nullptr)
	, m_irq_cb(*this)
{
}


//-------------------------------------------------
//  ~semihost_device - destructor
//-------------------------------------------------

semihost_device::~semihost_device()
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void semihost_device::device_start()
{
	// Resolve CPU device
	m_cpu = siblingdevice<cpu_device>(m_cpu_tag.c_str());
	if (!m_cpu)
		throw emu_fatalerror("semihost_device: CPU '%s' not found", m_cpu_tag.c_str());

	// Allocate work buffer for C library
	m_work_buffer = std::make_unique<u8[]>(WORK_BUFFER_SIZE);

	// Allocate and initialize backend state
	m_backend = new zbc_ansi_state_t();

	// Determine sandbox directory
	std::string sandbox = m_sandbox_dir.empty() ? get_default_sandbox_dir() : m_sandbox_dir;

	// Ensure sandbox directory exists
#ifdef _WIN32
	CreateDirectoryA(sandbox.c_str(), nullptr);
#else
	mkdir(sandbox.c_str(), 0755);
#endif

	// Initialize secure ANSI backend with sandbox
	zbc_ansi_init(m_backend, sandbox.c_str());

	// Allocate and initialize host state
	m_host = new zbc_host_state_t();

	// Set up memory operations
	zbc_host_mem_ops_t mem_ops = {
		mem_read_u8,
		mem_write_u8,
		mem_read_block,
		mem_write_block
	};

	// Initialize host with secure backend
	zbc_host_init(m_host, &mem_ops, this,
	              zbc_backend_ansi(), m_backend,
	              m_work_buffer.get(), WORK_BUFFER_SIZE);

	LOG("semihost: initialized with sandbox '%s'\n", sandbox.c_str());

	// Save state
	save_item(NAME(m_riff_ptr));
	save_item(NAME(m_irq_status));
	save_item(NAME(m_irq_enable));
	save_item(NAME(m_status));
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void semihost_device::device_reset()
{
	m_riff_ptr = 0;
	m_irq_status = 0;
	m_irq_enable = 0;  // IRQ disabled by default - guest must opt-in
	m_status = ZBC_STATUS_DEVICE_PRESENT;

	// Clear any pending IRQ
	if (!m_irq_cb.isunset())
		m_irq_cb(CLEAR_LINE);
}


//-------------------------------------------------
//  device_stop - device-specific shutdown
//-------------------------------------------------

void semihost_device::device_stop()
{
	// Clean up backend (closes open files)
	if (m_backend)
	{
		zbc_ansi_cleanup(m_backend);
		delete m_backend;
		m_backend = nullptr;
	}

	// Clean up host state
	if (m_host)
	{
		delete m_host;
		m_host = nullptr;
	}

	// Work buffer cleaned up by unique_ptr
}


//-------------------------------------------------
//  read - read from device registers
//-------------------------------------------------

u8 semihost_device::read(offs_t offset)
{
	osd_printf_info("SEMIHOST READ: offset=0x%02x\n", offset);
	if (offset >= ZBC_REG_SIZE)
		return 0xff;

	// Handle each register region
	if (offset < ZBC_REG_SIGNATURE + ZBC_SIGNATURE_SIZE)
	{
		// SIGNATURE: return "SEMIHOST" ASCII
		static const char sig[] = ZBC_SIGNATURE_STR;
		LOGREG("semihost: read SIGNATURE[%d] = 0x%02x ('%c')\n",
		       offset, u8(sig[offset]), sig[offset]);
		return u8(sig[offset]);
	}
	else if (offset < ZBC_REG_DOORBELL)
	{
		// RIFF_PTR: 16-byte pointer field (native endian)
		unsigned ptr_offset = offset - ZBC_REG_RIFF_PTR;
		u8 val = (m_riff_ptr >> (ptr_offset * 8)) & 0xff;
		LOGREG("semihost: read RIFF_PTR[%d] = 0x%02x\n", ptr_offset, val);
		return val;
	}
	else if (offset == ZBC_REG_DOORBELL)
	{
		// DOORBELL: write-only, reads return 0
		LOGREG("semihost: read DOORBELL (write-only, returning 0)\n");
		return 0;
	}
	else if (offset == ZBC_REG_IRQ_STATUS)
	{
		LOGREG("semihost: read IRQ_STATUS = 0x%02x\n", m_irq_status);
		return m_irq_status;
	}
	else if (offset == ZBC_REG_IRQ_ENABLE)
	{
		LOGREG("semihost: read IRQ_ENABLE = 0x%02x\n", m_irq_enable);
		return m_irq_enable;
	}
	else if (offset == ZBC_REG_IRQ_ACK)
	{
		// IRQ_ACK: write-only, reads return 0
		LOGREG("semihost: read IRQ_ACK (write-only, returning 0)\n");
		return 0;
	}
	else if (offset == ZBC_REG_STATUS)
	{
		LOGREG("semihost: read STATUS = 0x%02x\n", m_status);
		return m_status;
	}
	else
	{
		// Reserved
		LOGREG("semihost: read reserved[0x%02x] = 0x00\n", offset);
		return 0;
	}
}


//-------------------------------------------------
//  write - write to device registers
//-------------------------------------------------

void semihost_device::write(offs_t offset, u8 data)
{
	if (offset >= ZBC_REG_SIZE)
		return;

	// Handle each register region
	if (offset < ZBC_REG_SIGNATURE + ZBC_SIGNATURE_SIZE)
	{
		// SIGNATURE: read-only, ignore writes
		LOGREG("semihost: write SIGNATURE[%d] = 0x%02x (ignored, read-only)\n", offset, data);
	}
	else if (offset < ZBC_REG_DOORBELL)
	{
		// RIFF_PTR: 16-byte pointer field (native endian)
		unsigned ptr_offset = offset - ZBC_REG_RIFF_PTR;
		u64 mask = u64(0xff) << (ptr_offset * 8);
		m_riff_ptr = (m_riff_ptr & ~mask) | (u64(data) << (ptr_offset * 8));
		LOGREG("semihost: write RIFF_PTR[%d] = 0x%02x (ptr now 0x%016llx)\n",
		       ptr_offset, data, (unsigned long long)m_riff_ptr);
	}
	else if (offset == ZBC_REG_DOORBELL)
	{
		// DOORBELL: trigger request processing
		LOGREG("semihost: write DOORBELL = 0x%02x (triggering request at 0x%016llx)\n",
		       data, (unsigned long long)m_riff_ptr);
		process_request();
	}
	else if (offset == ZBC_REG_IRQ_STATUS)
	{
		// IRQ_STATUS: read-only, ignore writes
		LOGREG("semihost: write IRQ_STATUS = 0x%02x (ignored, read-only)\n", data);
	}
	else if (offset == ZBC_REG_IRQ_ENABLE)
	{
		LOGREG("semihost: write IRQ_ENABLE = 0x%02x\n", data);
		m_irq_enable = data;
		update_irq();
	}
	else if (offset == ZBC_REG_IRQ_ACK)
	{
		// IRQ_ACK: clear corresponding bits in IRQ_STATUS
		LOGREG("semihost: write IRQ_ACK = 0x%02x (clearing IRQ bits)\n", data);
		m_irq_status &= ~data;
		update_irq();
	}
	else if (offset == ZBC_REG_STATUS)
	{
		// STATUS: read-only, ignore writes
		LOGREG("semihost: write STATUS = 0x%02x (ignored, read-only)\n", data);
	}
	else
	{
		// Reserved
		LOGREG("semihost: write reserved[0x%02x] = 0x%02x (ignored)\n", offset, data);
	}
}


//-------------------------------------------------
//  process_request - process semihosting request
//-------------------------------------------------

void semihost_device::process_request()
{
	// Clear response-ready status before processing
	m_status &= ~ZBC_STATUS_RESPONSE_READY;

	// Call C library to process the RIFF request
	LOGREQUEST("semihost: processing request at address 0x%016llx\n", (unsigned long long)m_riff_ptr);

	int result = zbc_host_process(m_host, m_riff_ptr);

	LOGREQUEST("semihost: request complete, result = %d\n", result);

	// Update status
	m_status |= ZBC_STATUS_RESPONSE_READY;

	// Set IRQ status bit
	if (result == 0)
		m_irq_status |= ZBC_IRQ_RESPONSE_READY;
	else
		m_irq_status |= ZBC_IRQ_ERROR;

	// Assert IRQ if enabled
	update_irq();
}


//-------------------------------------------------
//  update_irq - update IRQ output based on status
//-------------------------------------------------

void semihost_device::update_irq()
{
	if (m_irq_cb.isunset())
		return;

	// IRQ is asserted if any enabled status bits are set
	bool irq_active = (m_irq_status & m_irq_enable) != 0;
	m_irq_cb(irq_active ? ASSERT_LINE : CLEAR_LINE);
}


//-------------------------------------------------
//  Memory access callbacks for C library
//-------------------------------------------------

u8 semihost_device::mem_read_u8(u64 addr, void *ctx)
{
	auto *dev = static_cast<semihost_device *>(ctx);
	return dev->m_cpu->space(AS_PROGRAM).read_byte(addr);
}

void semihost_device::mem_write_u8(u64 addr, u8 val, void *ctx)
{
	auto *dev = static_cast<semihost_device *>(ctx);
	dev->m_cpu->space(AS_PROGRAM).write_byte(addr, val);
}

void semihost_device::mem_read_block(void *dest, u64 addr, size_t size, void *ctx)
{
	auto *dev = static_cast<semihost_device *>(ctx);
	address_space &space = dev->m_cpu->space(AS_PROGRAM);
	u8 *dst = static_cast<u8 *>(dest);
	for (size_t i = 0; i < size; i++)
		dst[i] = space.read_byte(addr + i);
}

void semihost_device::mem_write_block(u64 addr, const void *src, size_t size, void *ctx)
{
	auto *dev = static_cast<semihost_device *>(ctx);
	address_space &space = dev->m_cpu->space(AS_PROGRAM);
	const u8 *s = static_cast<const u8 *>(src);
	for (size_t i = 0; i < size; i++)
		space.write_byte(addr + i, s[i]);
}


//-------------------------------------------------
//  get_default_sandbox_dir - cross-platform default
//-------------------------------------------------

std::string semihost_device::get_default_sandbox_dir()
{
#ifdef _WIN32
	char path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, path)))
	{
		return std::string(path) + "\\.mame\\semihost\\";
	}
	// Fallback
	const char *home = getenv("USERPROFILE");
	if (!home) home = getenv("HOMEDRIVE");
	return std::string(home ? home : "C:") + "\\.mame\\semihost\\";
#else
	const char *home = getenv("HOME");
	if (!home)
	{
		struct passwd *pw = getpwuid(getuid());
		if (pw)
			home = pw->pw_dir;
	}
	return std::string(home ? home : "/tmp") + "/.mame/semihost/";
#endif
}
