// license:BSD-3-Clause
// copyright-holders:John Byrd
/*********************************************************************

    elfload.cpp

    ELF executable loader for emulated systems

    Loads PT_LOAD segments from static ELF executables into guest
    memory, then resets the CPU. No relocation support - ELF must
    be fully resolved (ET_EXEC with no relocations).

*********************************************************************/

#include "emu.h"
#include "elfload.h"

//============================================================
// ELF constants
//============================================================

// ELF identification indices
enum : size_t {
	EI_MAG0    = 0,
	EI_MAG1    = 1,
	EI_MAG2    = 2,
	EI_MAG3    = 3,
	EI_CLASS   = 4,
	EI_DATA    = 5,
	EI_VERSION = 6,
	EI_NIDENT  = 16
};

// ELF magic
enum : uint8_t { ELFMAG0 = 0x7f, ELFMAG1 = 'E', ELFMAG2 = 'L', ELFMAG3 = 'F' };

// ELF class
enum : uint8_t { ELFCLASS32 = 1, ELFCLASS64 = 2 };

// ELF data encoding
enum : uint8_t { ELFDATA2LSB = 1, ELFDATA2MSB = 2 };

// ELF file types
enum : uint16_t { ET_REL = 1, ET_EXEC = 2, ET_DYN = 3 };

// Program header types
enum : uint32_t { PT_LOAD = 1, PT_DYNAMIC = 2, PT_TLS = 7 };

//============================================================
// ELF structures (portable, packed)
//============================================================

#pragma pack(push, 1)

struct Elf32_Ehdr {
	uint8_t  e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;
	uint32_t e_phoff;
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct Elf64_Ehdr {
	uint8_t  e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct Elf32_Phdr {
	uint32_t p_type;
	uint32_t p_offset;
	uint32_t p_vaddr;
	uint32_t p_paddr;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
};

struct Elf64_Phdr {
	uint32_t p_type;
	uint32_t p_flags;   // Note: different position than 32-bit
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

#pragma pack(pop)

static_assert(sizeof(Elf32_Ehdr) == 52, "Elf32_Ehdr size mismatch");
static_assert(sizeof(Elf64_Ehdr) == 64, "Elf64_Ehdr size mismatch");
static_assert(sizeof(Elf32_Phdr) == 32, "Elf32_Phdr size mismatch");
static_assert(sizeof(Elf64_Phdr) == 56, "Elf64_Phdr size mismatch");

//============================================================
// Endian-aware field access
//============================================================

template <typename T>
static T elf_read(T value, bool elf_big)
{
	if constexpr (sizeof(T) == 2)
		return elf_big ? big_endianize_int16(value) : little_endianize_int16(value);
	else if constexpr (sizeof(T) == 4)
		return elf_big ? big_endianize_int32(value) : little_endianize_int32(value);
	else if constexpr (sizeof(T) == 8)
		return elf_big ? big_endianize_int64(value) : little_endianize_int64(value);
	else
		return value;
}

// Device type definition
DEFINE_DEVICE_TYPE(ELFLOAD, elfload_image_device, "elfload", "ELF Loader")

elfload_image_device::elfload_image_device(const machine_config &mconfig,
	const char *tag, device_t *owner, uint32_t clock)
	: snapshot_image_device(mconfig, ELFLOAD, tag, owner, clock)
	, m_cpu(*this, finder_base::DUMMY_TAG)
{
}

void elfload_image_device::device_start()
{
	snapshot_image_device::device_start();
	set_load_callback(load_delegate(*this, FUNC(elfload_image_device::load_elf)));
}

std::pair<std::error_condition, std::string>
elfload_image_device::load_elf(snapshot_image_device &img)
{
	// Read entire file
	uint64_t size = img.length();
	if (size < 52)  // Minimum ELF32 header size
		return std::make_pair(image_error::INVALIDIMAGE, "File too small for ELF header");

	std::vector<uint8_t> data(size);
	if (img.fread(data.data(), size) != size)
		return std::make_pair(image_error::INVALIDIMAGE, "Failed to read file");

	// Check ELF magic
	if (data[0] != ELFMAG0 || data[1] != ELFMAG1 ||
		data[2] != ELFMAG2 || data[3] != ELFMAG3)
		return std::make_pair(image_error::INVALIDIMAGE, "Not an ELF file");

	// Get class (32/64-bit) and endianness
	uint8_t elf_class = data[4];
	uint8_t elf_data = data[5];

	if (elf_class != ELFCLASS32 && elf_class != ELFCLASS64)
		return std::make_pair(image_error::INVALIDIMAGE, "Unknown ELF class");
	if (elf_data != ELFDATA2LSB && elf_data != ELFDATA2MSB)
		return std::make_pair(image_error::INVALIDIMAGE, "Unknown ELF endianness");

	// Reset CPU FIRST - initialize memory before loading segments
	osd_printf_verbose("ELF: Resetting CPU '%s' before loading segments\n", m_cpu->tag());
	m_cpu->reset();
	osd_printf_verbose("ELF: CPU reset complete, now loading segments\n");

	bool success;

	if (elf_class == ELFCLASS32)
		success = parse_elf32(data.data(), size);
	else
		success = parse_elf64(data.data(), size);

	if (!success)
		return std::make_pair(image_error::INVALIDIMAGE, "Failed to parse ELF");

	osd_printf_verbose("ELF: All segments loaded\n");

	return std::make_pair(std::error_condition(), std::string());
}

bool elfload_image_device::parse_elf32(const uint8_t *data, size_t size)
{
	if (size < sizeof(Elf32_Ehdr))
		return false;

	const auto *ehdr = reinterpret_cast<const Elf32_Ehdr *>(data);
	const bool big = (ehdr->e_ident[EI_DATA] == ELFDATA2MSB);

	uint16_t e_type = elf_read(ehdr->e_type, big);
	if (e_type != ET_EXEC)
	{
		report_elf_type_error(e_type);
		return false;
	}

	uint32_t e_phoff = elf_read(ehdr->e_phoff, big);
	uint16_t e_phentsize = elf_read(ehdr->e_phentsize, big);
	uint16_t e_phnum = elf_read(ehdr->e_phnum, big);

	if (e_phentsize < sizeof(Elf32_Phdr))
		return false;
	if (e_phoff + uint64_t(e_phnum) * e_phentsize > size)
		return false;

	// First pass: check for unsupported segment types
	for (uint16_t i = 0; i < e_phnum; i++)
	{
		const auto *phdr = reinterpret_cast<const Elf32_Phdr *>(data + e_phoff + i * e_phentsize);
		uint32_t p_type = elf_read(phdr->p_type, big);

		if (p_type == PT_DYNAMIC)
		{
			osd_printf_error("ELF: Dynamic linking not supported. "
				"Rebuild with: -static -nostdlib -fno-pie\n");
			return false;
		}
	}

	// Second pass: load PT_LOAD and PT_TLS segments
	// PT_TLS is loaded like PT_LOAD - the C runtime handles TLS initialization
	for (uint16_t i = 0; i < e_phnum; i++)
	{
		const auto *phdr = reinterpret_cast<const Elf32_Phdr *>(data + e_phoff + i * e_phentsize);
		uint32_t p_type = elf_read(phdr->p_type, big);

		if (p_type != PT_LOAD && p_type != PT_TLS)
			continue;

		uint32_t p_offset = elf_read(phdr->p_offset, big);
		uint32_t p_vaddr = elf_read(phdr->p_vaddr, big);
		uint32_t p_filesz = elf_read(phdr->p_filesz, big);
		uint32_t p_memsz = elf_read(phdr->p_memsz, big);

		if (p_offset + p_filesz > size)
			return false;

		load_segment(p_vaddr, data + p_offset, p_filesz, p_memsz);
	}

	return true;
}

bool elfload_image_device::parse_elf64(const uint8_t *data, size_t size)
{
	if (size < sizeof(Elf64_Ehdr))
		return false;

	const auto *ehdr = reinterpret_cast<const Elf64_Ehdr *>(data);
	const bool big = (ehdr->e_ident[EI_DATA] == ELFDATA2MSB);

	uint16_t e_type = elf_read(ehdr->e_type, big);
	if (e_type != ET_EXEC)
	{
		report_elf_type_error(e_type);
		return false;
	}

	uint64_t e_phoff = elf_read(ehdr->e_phoff, big);
	uint16_t e_phentsize = elf_read(ehdr->e_phentsize, big);
	uint16_t e_phnum = elf_read(ehdr->e_phnum, big);

	if (e_phentsize < sizeof(Elf64_Phdr))
		return false;
	if (e_phoff + uint64_t(e_phnum) * e_phentsize > size)
		return false;

	// First pass: check for unsupported segment types
	for (uint16_t i = 0; i < e_phnum; i++)
	{
		const auto *phdr = reinterpret_cast<const Elf64_Phdr *>(data + e_phoff + i * e_phentsize);
		uint32_t p_type = elf_read(phdr->p_type, big);

		if (p_type == PT_DYNAMIC)
		{
			osd_printf_error("ELF: Dynamic linking not supported. "
				"Rebuild with: -static -nostdlib -fno-pie\n");
			return false;
		}
	}

	// Second pass: load PT_LOAD and PT_TLS segments
	// PT_TLS is loaded like PT_LOAD - the C runtime handles TLS initialization
	for (uint16_t i = 0; i < e_phnum; i++)
	{
		const auto *phdr = reinterpret_cast<const Elf64_Phdr *>(data + e_phoff + i * e_phentsize);
		uint32_t p_type = elf_read(phdr->p_type, big);

		if (p_type != PT_LOAD && p_type != PT_TLS)
			continue;

		uint64_t p_offset = elf_read(phdr->p_offset, big);
		uint64_t p_vaddr = elf_read(phdr->p_vaddr, big);
		uint64_t p_filesz = elf_read(phdr->p_filesz, big);
		uint64_t p_memsz = elf_read(phdr->p_memsz, big);

		if (p_offset + p_filesz > size)
			return false;

		load_segment(p_vaddr, data + p_offset, p_filesz, p_memsz);
	}

	return true;
}

void elfload_image_device::load_segment(uint64_t vaddr, const uint8_t *data,
	size_t filesz, size_t memsz)
{
	address_space &space = m_cpu->space(AS_PROGRAM);

	osd_printf_verbose("ELF: Loading segment at 0x%08x, filesz=0x%x, memsz=0x%x\n",
		(uint32_t)vaddr, (uint32_t)filesz, (uint32_t)memsz);
	osd_printf_verbose("ELF: Address space: %d-bit, mask=0x%llx\n",
		space.addr_width(), (unsigned long long)space.addrmask());

	// Show first 16 bytes of data
	osd_printf_verbose("ELF: First bytes to write:");
	for (size_t i = 0; i < std::min(filesz, size_t(16)); i++)
		osd_printf_verbose(" %02x", data[i]);
	osd_printf_verbose("\n");

	// Try to get direct memory pointer for faster writes
	uint8_t *write_ptr = reinterpret_cast<uint8_t*>(space.get_write_ptr(vaddr));

	if (write_ptr && filesz > 0)
	{
		osd_printf_verbose("ELF: Using direct memory write to %p\n", (void*)write_ptr);
		memcpy(write_ptr, data, filesz);
		// Zero-fill BSS
		if (memsz > filesz)
			memset(write_ptr + filesz, 0, memsz - filesz);
	}
	else
	{
		osd_printf_verbose("ELF: Using byte-by-byte write (no direct pointer)\n");
		// Copy file data
		for (size_t i = 0; i < filesz; i++)
			space.write_byte(vaddr + i, data[i]);
		// Zero-fill BSS
		if (memsz > filesz) {
			osd_printf_verbose("ELF: Zero-filling BSS from 0x%08x to 0x%08x (%u bytes)\n",
				(uint32_t)(vaddr + filesz), (uint32_t)(vaddr + memsz), (uint32_t)(memsz - filesz));
		}
		for (size_t i = filesz; i < memsz; i++)
			space.write_byte(vaddr + i, 0);
	}

	osd_printf_verbose("ELF: Segment loaded successfully\n");
}

void elfload_image_device::report_elf_type_error(uint16_t e_type)
{
	if (e_type == ET_REL)
		osd_printf_error("ELF error: Relocatable file (ET_REL) not supported. "
			"Link with: -static -fno-pie\n");
	else if (e_type == ET_DYN)
		osd_printf_error("ELF error: Shared object (ET_DYN) not supported. "
			"Link with: -static -fno-pie -no-pie\n");
	else
		osd_printf_error("ELF error: Unsupported file type %d (need ET_EXEC=2)\n", e_type);
}
