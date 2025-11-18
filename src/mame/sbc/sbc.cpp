// license:BSD-3-Clause
// copyright-holders:MAME Team
/***************************************************************************

    Single Board Computer Templates

    Minimal CPU systems for testing and development across all architectures.
    Each SBC includes:
    - RAM (size determined by CPU address width)
    - MC6847 VDG text display (32x16)
    - Quickload support for loading programs

    Design Philosophy:
    This module uses C++ templates to eliminate code duplication across
    CPU architectures. A single template class (sbc_state) is instantiated
    for each CPU type, with compile-time customization of load address,
    clock speed, and video RAM placement. CPU-specific initialization
    (reset vectors, exception tables) is handled via template specialization.

    The DEFINE_SBC macro generates complete machine variants from a single
    line, creating unique classes and registering them with MAME.

    Limitations:
    - No ROM (program must be loaded via -quik)
    - No I/O ports (memory-mapped video only)
    - Video RAM is fixed at calculated address (may conflict with some programs)

    Usage:
    mame sbc6502 -quik program.bin
    mame sbcz80 -quik program.bin
    mame sbc68000 -quik program.bin
    etc.

    The program will be loaded at the configured address (default 0x0200)
    and executed.

***************************************************************************/

#include "emu.h"
#include "emupal.h"
#include "screen.h"

// ============================================================================
// CPU HEADERS - Comprehensive collection of all MAME CPU architectures
// ============================================================================

// Classic 8-bit CPUs
#include "cpu/i8085/i8085.h"           // Intel 8080/8085
#include "cpu/m6502/m6502.h"           // MOS 6502 family
#include "cpu/m6502/m6503.h"
#include "cpu/m6502/m6504.h"
#include "cpu/m6502/m6507.h"
#include "cpu/m6502/m6509.h"
#include "cpu/m6502/m6510.h"
#include "cpu/m6502/m6510t.h"
#include "cpu/m6502/m4510.h"
#include "cpu/m6502/m65ce02.h"
#include "cpu/m6502/m7501.h"
#include "cpu/m6502/m8502.h"
#include "cpu/m6502/r65c02.h"
#include "cpu/m6502/r65c19.h"
#include "cpu/m6502/w65c02.h"
#include "cpu/m6502/w65c02s.h"
#include "cpu/m6502/g65sc02.h"
#include "cpu/m6502/deco16.h"
#include "cpu/m6502/m6500_1.h"
#include "cpu/m6502/rp2a03.h"
#include "cpu/m6502/xavix.h"
#include "cpu/m6502/xavix2000.h"
#include "cpu/m6502/st2204.h"
#include "cpu/m6502/st2205u.h"
#include "cpu/m6502/m5074x.h"
#include "cpu/m6502/m50734.h"
#include "cpu/m6502/m3745x.h"
#include "cpu/m6502/m37640.h"
#include "cpu/m6502/gew7.h"
#include "cpu/m6502/gew12.h"
#include "cpu/m6800/m6800.h"           // Motorola 6800 family
#include "cpu/m6800/m6801.h"
#include "cpu/m6809/m6809.h"           // Motorola 6809 family
#include "cpu/m6809/hd6309.h"
#include "cpu/m6809/konami.h"
#include "cpu/z80/z80.h"               // Zilog Z80 family
#include "cpu/z80/z80n.h"
#include "cpu/z80/ez80.h"
#include "cpu/z80/r800.h"
#include "cpu/z80/nsc800.h"
#include "cpu/z80/mc8123.h"
#include "cpu/z80/kl5c80a12.h"
#include "cpu/z80/kl5c80a16.h"
#include "cpu/z80/lz8420m.h"
#include "cpu/z80/tmpz84c011.h"
#include "cpu/z80/tmpz84c015.h"
#include "cpu/z80/z84c015.h"
#include "cpu/z80/t6a84.h"
#include "cpu/z80/kp63.h"
#include "cpu/z80/kp64.h"
#include "cpu/z80/kp69.h"
#include "cpu/z80/ky80.h"
#include "cpu/z180/z180.h"             // Z180 family
#include "cpu/z180/hd647180x.h"

// 16-bit CPUs
#include "cpu/i86/i86.h"               // Intel 8086 family
#include "cpu/i86/i186.h"
#include "cpu/i86/i286.h"
#include "cpu/nec/nec.h"               // NEC V20/V30 family
#include "cpu/nec/v25.h"
#include "cpu/nec/v5x.h"
// #include "cpu/v30mz/v30mz.h"          // Conflicts with nec.h (enum NEC_PC, etc.)
#include "cpu/z8000/z8000.h"           // Zilog Z8000
#include "cpu/g65816/g65816.h"         // WDC 65816 (16-bit 6502)
#include "cpu/m37710/m37710.h"         // Mitsubishi M377xx

// 32-bit RISC CPUs
#include "cpu/arm/arm.h"               // ARM family
#include "cpu/arm7/arm7.h"
#include "cpu/arm7/lpc210x.h"
#include "cpu/arm7/upd800468.h"
#include "cpu/mips/mips1.h"            // MIPS I family
#include "cpu/mips/mips3.h"            // MIPS III/IV family
#include "cpu/mips/r4000.h"
#include "cpu/powerpc/ppc.h"           // PowerPC family
#include "cpu/sparc/sparc.h"           // SPARC family
#include "cpu/alpha/alpha.h"           // DEC Alpha
#include "cpu/i860/i860.h"             // Intel i860
#include "cpu/i960/i960.h"             // Intel i960
#include "cpu/clipper/clipper.h"       // Clipper RISC
#include "cpu/se3208/se3208.h"         // ADChips SE3208
// #include "cpu/am29000/am29000.h"       // AMD 29000 - Conflicts with clipper.h (EXCEPTION_TRACE)
#include "cpu/romp/romp.h"             // IBM ROMP
#include "cpu/score/score.h"           // Sunplus S+core

// 32-bit CISC CPUs
#include "cpu/i386/i386.h"             // Intel x86 (386+)
#include "cpu/i386/athlon.h"           // AMD Athlon
#include "cpu/m68000/m68000.h"         // Motorola 68000 family
#include "cpu/m68000/m68008.h"
#include "cpu/m68000/m68010.h"
#include "cpu/m68000/m68020.h"
#include "cpu/m68000/m68030.h"
#include "cpu/m68000/m68040.h"
#include "cpu/m68000/mcf5206e.h"       // ColdFire
#include "cpu/ns32000/ns32000.h"       // NS32000 family
#include "cpu/vax/vax.h"               // DEC VAX
#include "cpu/we32000/we32100.h"       // WE32100
#include "cpu/v60/v60.h"               // NEC V60/V70
#include "cpu/m88000/m88000.h"         // Motorola 88000

// DSP Processors
#include "cpu/adsp2100/adsp2100.h"     // Analog Devices ADSP-21xx
#include "cpu/sharc/sharc.h"           // Analog Devices SHARC
#include "cpu/dsp16/dsp16.h"           // AT&T DSP16
#include "cpu/dsp32/dsp32.h"           // AT&T DSP32
#include "cpu/dsp56000/dsp56000.h"     // Motorola DSP56000
#include "cpu/dsp563xx/dsp56303.h"     // Motorola DSP563xx family
#include "cpu/dsp563xx/dsp56311.h"
#include "cpu/dsp563xx/dsp56362.h"
#include "cpu/dsp563xx/dsp56364.h"
#include "cpu/tms32010/tms32010.h"     // TI TMS320 family
#include "cpu/tms32025/tms32025.h"
#include "cpu/tms32031/tms32031.h"
#include "cpu/tms32051/tms32051.h"
#include "cpu/tms32082/tms32082.h"
#include "cpu/tms34010/tms34010.h"     // TI TMS340x0 graphics
#include "cpu/tms57002/tms57002.h"
#include "cpu/upd7725/upd7725.h"       // NEC uPD77xx DSP
#include "cpu/mb86233/mb86233.h"       // Fujitsu MB86233
#include "cpu/mb86235/mb86235.h"       // Fujitsu MB86235
#include "cpu/es5510/es5510.h"         // Ensoniq ES5510
#include "cpu/dspp/dspp.h"             // 3DO DSPP
#include "cpu/ks0164/ks0164.h"         // Samsung KS0164

// Microcontrollers (8051 family)
#include "cpu/mcs51/i8051.h"
#include "cpu/mcs51/i8052.h"
#include "cpu/mcs51/i80c51.h"
#include "cpu/mcs51/i80c52.h"
#include "cpu/mcs51/ds5002fp.h"
#include "cpu/mcs51/sab80c535.h"
#include "cpu/axc51/axc51.h"           // Actions AX208

// Microcontrollers (PIC family)
#include "cpu/pic16c5x/pic16c5x.h"
#include "cpu/pic16c62x/pic16c62x.h"
#include "cpu/pic16x8x/pic16x8x.h"
#include "cpu/pic17/pic17c4x.h"

// Microcontrollers (AVR)
#include "cpu/avr8/avr8.h"

// Microcontrollers (68HC family)
#include "cpu/mc68hc11/mc68hc11.h"
#include "cpu/m68hc16/m68hc16z.h"
#include "cpu/m6805/m6805.h"
#include "cpu/m6805/m68705.h"
#include "cpu/m6805/m68hc05.h"
#include "cpu/m6805/m68hc05e1.h"
#include "cpu/m6805/m68hc05pge.h"
#include "cpu/m6805/hd6305.h"

// Microcontrollers (MCS-48 family)
#include "cpu/mcs48/mcs48.h"

// Microcontrollers (MCS-96 family)
#include "cpu/mcs96/i8x9x.h"

// Microcontrollers (H8 family)
#include "cpu/h8/h83002.h"
#include "cpu/h8/h83003.h"
#include "cpu/h8/h83006.h"
#include "cpu/h8/h83008.h"
#include "cpu/h8/h83032.h"
#include "cpu/h8/h83042.h"
#include "cpu/h8/h83048.h"
#include "cpu/h8/h83217.h"
#include "cpu/h8/h8325.h"
#include "cpu/h8/h83337.h"
#include "cpu/h8/h8s2245.h"
#include "cpu/h8/h8s2319.h"
#include "cpu/h8/h8s2329.h"
#include "cpu/h8/h8s2357.h"
#include "cpu/h8/h8s2655.h"
#include "cpu/h8/gt913.h"
#include "cpu/h8500/h8510.h"
#include "cpu/h8500/h8520.h"
#include "cpu/h8500/h8532.h"
#include "cpu/h8500/h8534.h"

// Microcontrollers (SuperH family)
#include "cpu/sh/sh4.h"
#include "cpu/sh/sh7014.h"
#include "cpu/sh/sh7021.h"
#include "cpu/sh/sh7032.h"
#include "cpu/sh/sh7042.h"
#include "cpu/sh/sh7604.h"

// Microcontrollers (NEC uPD7xxx family)
#include "cpu/upd7810/upd7810.h"
#include "cpu/upd78k/upd78k0.h"
#include "cpu/upd78k/upd78k2.h"
#include "cpu/upd78k/upd78k3.h"
#include "cpu/upd78k/upd78k4.h"

// Microcontrollers (Toshiba TLCS family)
#include "cpu/tlcs870/tlcs870.h"
#include "cpu/tlcs90/tlcs90.h"
#include "cpu/tlcs900/tmp94c241.h"
#include "cpu/tlcs900/tmp95c061.h"
#include "cpu/tlcs900/tmp95c063.h"
#include "cpu/tlcs900/tmp96c141.h"

// Microcontrollers (Fujitsu F2MC family)
#include "cpu/f2mc16/mb90570.h"
#include "cpu/f2mc16/mb90610a.h"
#include "cpu/f2mc16/mb90640a.h"

// Microcontrollers (Panasonic/Mitsubishi)
#include "cpu/mn10200/mn10200.h"
#include "cpu/mn1400/mn1400.h"
#include "cpu/mn1880/mn1880.h"
#include "cpu/olms66k/msm665xx.h"

// Microcontrollers (Epson)
#include "cpu/c33/s1c33209.h"
#include "cpu/e0c6200/e0c6s46.h"

// Microcontrollers (ST)
#include "cpu/st62xx/st62xx.h"
#include "cpu/st9/st905x.h"

// Microcontrollers (Sanyo)
#include "cpu/lc8670/lc8670.h"

// Microcontrollers (Misc)
#include "cpu/xa/xa.h"                 // Philips XA
#include "cpu/h6280/h6280.h"           // Hudson HuC6280
#include "cpu/v810/v810.h"             // NEC V810
#include "cpu/fr/fr.h"                 // Fujitsu FR
#include "cpu/cr16b/cr16b.h"           // CompactRISC
#include "cpu/nios2/nios2.h"           // Altera Nios II
#include "cpu/xtensa/xtensa.h"         // Tensilica Xtensa
#include "cpu/e132xs/e132xs.h"         // Hyperstone E1-32XS
#include "cpu/bcp/dp8344.h"            // NS DP8344
#include "cpu/hpc/hpc.h"               // NS HPC

// 4-bit Microcontrollers
#include "cpu/mcs40/mcs40.h"           // Intel 4004/4040
#include "cpu/cop400/cop400.h"         // National COP400
#include "cpu/tms1000/tms0270.h"       // TI TMS1000 family
#include "cpu/tms1000/tms0970.h"
#include "cpu/tms1000/tms0980.h"
#include "cpu/tms1000/tms1000.h"
#include "cpu/tms1000/tms1000c.h"
#include "cpu/tms1000/tms1100.h"
#include "cpu/tms1000/tms1400.h"
#include "cpu/tms1000/tms2100.h"
#include "cpu/tms1000/tms2400.h"
#include "cpu/tms1000/tp0320.h"
#include "cpu/tms1000/smc1102.h"
#include "cpu/hmcs40/hmcs40.h"         // Hitachi HMCS40
#include "cpu/hmcs400/hmcs400.h"       // Hitachi HMCS400
#include "cpu/ucom4/ucom4.h"           // NEC uCOM-4
#include "cpu/pps4/pps4.h"             // Rockwell PPS-4
#include "cpu/pps41/mm75.h"            // NS MM75/76/77/78
#include "cpu/pps41/mm76.h"
#include "cpu/pps41/mm78.h"
#include "cpu/pps41/mm78la.h"
#include "cpu/amis2000/amis2000.h"     // AMI S2000
#include "cpu/sm510/sm500.h"           // Sharp SM5xx family
#include "cpu/sm510/sm510.h"
#include "cpu/sm510/sm511.h"
#include "cpu/sm510/sm530.h"
#include "cpu/sm510/sm590.h"
#include "cpu/sm510/sm5a.h"
#include "cpu/mb88xx/mb88xx.h"         // Fujitsu MB88xx
#include "cpu/melps4/m58846.h"         // Mitsubishi MELPS-4
#include "cpu/rw5000/a5000.h"          // Rockwell calculator CPUs
#include "cpu/rw5000/a5500.h"
#include "cpu/rw5000/a5900.h"
#include "cpu/rw5000/b5000.h"
#include "cpu/rw5000/b5500.h"
#include "cpu/rw5000/b6000.h"
#include "cpu/rw5000/b6100.h"
#include "cpu/ht1130/ht1130.h"         // Holtek HT11xx

// Game/Graphics CPUs
#include "cpu/jaguar/jaguar.h"         // Atari Jaguar GPU/DSP
#include "cpu/rsp/rsp.h"               // N64 RSP
#include "cpu/superfx/superfx.h"       // SNES SuperFX
#include "cpu/spc700/spc700.h"         // SNES SPC700
#include "cpu/psx/psx.h"               // PlayStation
#include "cpu/scudsp/scudsp.h"         // Saturn SCU DSP
#include "cpu/ccpu/ccpu.h"             // Cinematronics CPU
#include "cpu/cp1610/cp1610.h"         // Intellivision
#include "cpu/esrip/esrip.h"           // Entertainment Sciences RIP
#include "cpu/ssp1601/ssp1601.h"       // Samsung SSP1601
#include "cpu/nuon/nuon.h"             // VM Labs Nuon
#include "cpu/cubeqcpu/cubeqcpu.h"     // Cube Quest
#include "cpu/unsp/unsp.h"             // SunPlus unSP
#include "cpu/lr35902/lr35902.h"       // Game Boy CPU
#include "cpu/minx/minx.h"             // Pokemon Mini
#include "cpu/sm8500/sm8500.h"         // Sharp SM8500
#include "cpu/xavix2/xavix2.h"         // SSD XaviX2
#include "cpu/arcompact/arcompact.h"   // ARCompact
#include "cpu/arc/arc.h"               // ARC
#include "cpu/sonix16/sonix16.h"       // Sonix 16-bit

// Vintage/Historic CPUs
#include "cpu/pdp1/pdp1.h"             // DEC PDP-1
#include "cpu/pdp8/pdp8.h"             // DEC PDP-8
#include "cpu/pdp8/hd6120.h"
#include "cpu/t11/t11.h"               // DEC T-11
#include "cpu/ssem/ssem.h"             // Manchester Baby
#include "cpu/tx0/tx0.h"               // MIT TX-0
#include "cpu/alto2/alto2cpu.h"        // Xerox Alto II
#include "cpu/apexc/apexc.h"           // APEXC
#include "cpu/mk1/mk1.h"               // Ferranti Mark 1
#include "cpu/gigatron/gigatron.h"     // Gigatron
#include "cpu/patinhofeio/patinhofeio_cpu.h"  // Patinho Feio
#include "cpu/ie15/ie15.h"             // Soviet IE-15
#include "cpu/mpk1839/kl1839vm1.h"     // Soviet KL1839VM1

// Calculator/Terminal CPUs
#include "cpu/saturn/saturn.h"         // HP Saturn
#include "cpu/hcd62121/hcd62121.h"     // Hitachi HCD62121
#include "cpu/sc61860/sc61860.h"       // Sharp SC61860
#include "cpu/hd61700/hd61700.h"       // Hitachi HD61700
#include "cpu/capricorn/capricorn.h"   // HP Capricorn
#include "cpu/nanoprocessor/nanoprocessor.h"  // HP Nanoprocessor
#include "cpu/hphybrid/hphybrid.h"     // HP Hybrid
#include "cpu/palm/palm.h"             // Palm

// Misc CPUs
#include "cpu/i8008/i8008.h"           // Intel 8008
#include "cpu/i8089/i8089.h"           // Intel 8089 I/O processor
#include "cpu/f8/f8.h"                 // Fairchild F8
#include "cpu/s2650/s2650.h"           // Signetics 2650
#include "cpu/scmp/scmp.h"             // SC/MP
#include "cpu/lh5801/lh5801.h"         // Sharp LH5801
#include "cpu/8x300/8x300.h"           // Signetics 8X300
#include "cpu/asap/asap.h"             // ASAP
#include "cpu/pace/pace.h"             // NS PACE
#include "cpu/mipsx/mipsx.h"           // MIPS-X
#include "cpu/tms9900/tms9900.h"       // TI TMS9900 family
#include "cpu/tms9900/tms9980a.h"
#include "cpu/tms9900/tms9995.h"
#include "cpu/tms9900/ti990_10.h"
#include "cpu/tms7000/tms7000.h"       // TI TMS7000
#include "cpu/z8/z8.h"                 // Zilog Z8
#include "cpu/cops1/mm5799.h"          // NS COPS I
#include "cpu/cosmac/cosmac.h"         // RCA COSMAC
#include "cpu/upd177x/upd177x.h"       // NEC uPD1771C
#include "cpu/upd777/upd777.h"         // NEC uPD777
#include "cpu/diablo/diablo1300.h"     // Xerox Diablo
#include "cpu/rx01/rx01.h"             // DEC RX01
#include "cpu/vt50/vt50.h"             // DEC VT50/VT52
#include "cpu/vt61/vt61.h"             // DEC VT61
#include "cpu/h16/hd641016.h"          // Hitachi HD641016

#include "imagedev/snapquik.h"
#include "video/mc6847.h"

namespace {

// Template parameters control compile-time instantiation of CPU-specific variants:
//
// - CPU_TYPE: The CPU device class (e.g., m6502_device, z80_device)
//   Must be a cpu_device subclass. Used for template specialization matching
//   in init_cpu_for_idle<CPU_TYPE, LOAD_ADDR>(). Address space width is
//   queried from this device at runtime.
//
// - LOAD_ADDR: Where quickload programs are loaded (default 0x0200)
//   Default of 0x0200 avoids low memory conflicts (zero page, reset vectors).
//   Must be less than VRAM_ADDR to prevent program/video memory collision.
//
// - CPU_SPEED: CPU clock frequency in Hz (default 10 MHz)
//   Affects emulation speed but not functional behavior. MC6847 video timing
//   is independent (fixed at 4.433619 MHz PAL).
//
// - VRAM_ADDR: Video RAM base address (default 0 = auto-calculate)
//   When 0, VRAM is placed near top of address space with 256-byte gap for
//   vectors/high RAM. Explicit values override auto-calculation.
template <typename CPU_TYPE, uint32_t LOAD_ADDR = 0x0200,
          uint32_t CPU_SPEED = 10'000'000, uint32_t VRAM_ADDR = 0>
class sbc_state : public driver_device {
  public:
	sbc_state(const machine_config &mconfig, device_type type, const char *tag)
	    : driver_device(mconfig, type, tag), m_maincpu(*this, "maincpu"),
	      m_vdg(*this, "vdg"),
	      m_videoram(*this, "videoram", 0x200, ENDIANNESS_LITTLE) {}

	template <typename DEVICE_TYPE>
	void sbc(machine_config &config, DEVICE_TYPE const &cpu_type) ATTR_COLD;

  protected:
	virtual void machine_start() override;
	virtual void machine_reset() override ATTR_COLD;

  private:
	required_device<cpu_device> m_maincpu;
	required_device<mc6847_base_device> m_vdg;

	// Video RAM must be accessible as bytes regardless of CPU bus width.
	// memory_share_creator ensures 8-bit access even on 16/32-bit CPUs,
	// allowing the MC6847 VDG to read character data byte-by-byte.
	// Installed in machine_start() using install_ram() to force byte granularity.
	memory_share_creator<uint8_t> m_videoram;

	void mem_map(address_map &map) ATTR_COLD;
	void init_screen();
	void chrout(char c);
	void print_word(const char *word);
	void print_sentence(const char *text);
	void center_line(const char *text);
	void scroll_up();

	DECLARE_QUICKLOAD_LOAD_MEMBER(quickload_cb);

	uint8_t vdg_videoram_r(offs_t offset);

	// Calculate video RAM address based on CPU address space size.
	// Strategy: Place VRAM near top of address space to maximize contiguous
	// low memory for programs, while reserving 256 bytes above VRAM for
	// CPU-specific vectors (6502 reset/IRQ, Z80 IM2, etc).
	uint32_t get_vram_addr() const {
		if (VRAM_ADDR != 0)
			return VRAM_ADDR; // Explicit override

		// Auto-calculate: place VRAM near top of address space
		// Leave 256 bytes for high RAM/vectors
		if (!m_maincpu || !m_maincpu->has_space(AS_PROGRAM))
			return 0xFD00; // Fallback for 16-bit

		uint8_t addr_bits = m_maincpu->space(AS_PROGRAM).addr_width();

		if (addr_bits <= 16)
			return 0xFD00; // 16-bit: 0xFD00-0xFEFF, high RAM at 0xFF00-0xFFFF
		else if (addr_bits <= 24)
			return 0xFFFD00; // 24-bit
		else
			return 0xFFFFFD00; // 32-bit+
	}

	uint32_t get_ram_size() const {
		if (!m_maincpu || !m_maincpu->has_space(AS_PROGRAM))
			return 0x10000; // Fallback

		uint8_t addr_bits = m_maincpu->space(AS_PROGRAM).addr_width();
		return (1ULL << addr_bits);
	}

	int m_cursor_pos;
	static constexpr int LINE_WIDTH = 32;
	static constexpr int SCREEN_HEIGHT = 16;
	static constexpr int SCREEN_SIZE = 0x200;
};

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::mem_map(
    address_map &map) {
	map.unmap_value_high(); // Unmapped reads return 0xFF (floating bus)

	uint32_t vram_addr = get_vram_addr();
	uint32_t ram_size = get_ram_size();

	// Map RAM in sections around video RAM (creates fragmented address space)
	if (vram_addr > 0)
		map(0x0000, vram_addr - 1).ram();

	// Video RAM (512 bytes) gap - installed later in machine_start()
	// Must use install_ram() instead of map().ram() to force 8-bit access
	// on CPUs with 16/32-bit bus widths. The MC6847 requires byte-level reads.

	uint32_t after_vram = vram_addr + 0x200;
	if (after_vram < ram_size)
		map(after_vram, ram_size - 1).ram();
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
uint8_t sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::vdg_videoram_r(
    offs_t offset) {
	// MC6847 VDG reads character codes from video RAM for display.
	// Called ~15,000 times/second as VDG scans the 32x16 character grid.
	return m_videoram[offset & 0x1ff]; // Mask to 512-byte range
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::scroll_up() {
	for (int i = 0; i < SCREEN_SIZE - LINE_WIDTH; i++)
		m_videoram[i] = m_videoram[i + LINE_WIDTH];

	for (int i = SCREEN_SIZE - LINE_WIDTH; i < SCREEN_SIZE; i++)
		m_videoram[i] = 0x20;

	m_cursor_pos -= LINE_WIDTH;
	if (m_cursor_pos < 0)
		m_cursor_pos = 0;
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::chrout(char c) {
	if (c == '\r' || c == '\n') {
		m_cursor_pos = ((m_cursor_pos / LINE_WIDTH) + 1) * LINE_WIDTH;
		if (m_cursor_pos >= SCREEN_SIZE) {
			scroll_up();
			m_cursor_pos = SCREEN_SIZE - LINE_WIDTH;
		}
	} else {
		m_videoram[m_cursor_pos] = toupper(c);
		m_cursor_pos++;
		if (m_cursor_pos >= SCREEN_SIZE) {
			scroll_up();
			m_cursor_pos = SCREEN_SIZE - LINE_WIDTH;
		}
	}
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::print_word(
    const char *word) {
	int word_len = strlen(word);
	int col = m_cursor_pos % LINE_WIDTH;

	int needed = word_len;
	if (col > 0)
		needed++;

	if (col > 0 && col + needed > LINE_WIDTH) {
		chrout('\r');
		col = 0;
	}

	if (col > 0)
		chrout(' ');

	for (int i = 0; i < word_len; i++)
		chrout(word[i]);
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::print_sentence(
    const char *text) {
	char word[64];
	int word_idx = 0;

	for (const char *p = text; *p; p++) {
		if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
			if (word_idx > 0) {
				word[word_idx] = '\0';
				print_word(word);
				word_idx = 0;
			}
		} else {
			if (word_idx < 63)
				word[word_idx++] = *p;
		}
	}

	if (word_idx > 0) {
		word[word_idx] = '\0';
		print_word(word);
	}
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::center_line(
    const char *text) {
	int len = strlen(text);
	int padding = (LINE_WIDTH - len) / 2;

	for (int i = 0; i < padding; i++)
		chrout(' ');

	for (int i = 0; i < len; i++)
		chrout(text[i]);

	chrout('\r');
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::init_screen() {
	for (int i = 0; i < 0x200; i++)
		m_videoram[i] = 0x20;

	m_cursor_pos = 0;

	center_line("Single board computer");
	center_line(m_maincpu->name());
	center_line("");
	center_line("github.com/johnwbyrd/semihost");
	center_line("");

	// Display memory configuration
	char addr_buf[64];
	uint32_t vram_addr = get_vram_addr();
	uint32_t available_ram = vram_addr - LOAD_ADDR;

	snprintf(addr_buf, sizeof(addr_buf), "Load address: 0x%X", LOAD_ADDR);
	center_line(addr_buf);

	snprintf(addr_buf, sizeof(addr_buf), "Available RAM: %u bytes",
	         available_ram);
	center_line(addr_buf);

	snprintf(addr_buf, sizeof(addr_buf), "Video RAM: 0x%X-0x%X",
	         vram_addr, vram_addr + 0x1FF);
	center_line(addr_buf);

	center_line("");

	print_sentence("This is a minimal system with RAM and a "
	               "MC6847 video display. "
	               "Load a headerless binary in MAME using the -quik option. "
	               "The program will be loaded and executed. "
	               "Happy coding!");
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::machine_start() {
	// Install 8-bit video RAM at the calculated address.
	// CRITICAL: Must use install_ram() here (not map().ram() in mem_map())
	// to force byte-granular access on 16/32-bit CPUs. The MC6847 VDG
	// always reads bytes, but map().ram() creates CPU-width memory.
	uint32_t vram_addr = get_vram_addr();
	m_maincpu->space(AS_PROGRAM)
	    .install_ram(vram_addr, vram_addr + 0x1FF, m_videoram.target());
}

// CPU-specific idle initialization using template specialization.
//
// This uses C++ template specialization to provide CPU-specific boot code
// while maintaining a generic interface. The compiler selects the most
// specific matching template at compile time based on CPU_TYPE and LOAD_ADDR.
//
// Each specialization writes reset vectors and idle loops appropriate for
// the CPU architecture, allowing the system to boot and wait for quickload.
template <typename CPU_TYPE, uint32_t LOAD_ADDR>
void init_cpu_for_idle(address_space &space) {
	// Default: no special initialization needed for CPUs without specialization
}

// 6502 specialization: Set reset vector and create idle loop
template <> void init_cpu_for_idle<m6502_device, 0x0200>(address_space &space) {
	// 6502 reads reset vector from 0xFFFC-0xFFFD (little-endian)
	space.write_byte(0xfffc, 0x00);
	space.write_byte(0xfffd, 0x02); // Points to 0x0200

	// Idle loop at load address: JMP $0200 (infinite loop)
	space.write_byte(0x0200, 0x4c); // JMP absolute opcode
	space.write_byte(0x0201, 0x00);
	space.write_byte(0x0202, 0x02);
}

// Z80 specialization: Boot code and NMI handler
template <> void init_cpu_for_idle<z80_device, 0x0200>(address_space &space) {
	// Z80 starts at 0x0000 on reset, jump to load address
	space.write_byte(0x0000, 0xc3); // JP opcode
	space.write_byte(0x0001, 0x00);
	space.write_byte(0x0002, 0x02); // JP $0200

	// NMI handler at 0x0066 (MC6847 vsync generates NMI)
	space.write_byte(0x0066, 0xed); // RETN opcode
	space.write_byte(0x0067, 0x45); // (2-byte instruction)

	// Idle loop: JR -2 (relative jump to self)
	space.write_byte(0x0200, 0x18); // JR opcode
	space.write_byte(0x0201, 0xfe); // Offset -2
}

// 68000 specialization: Exception vectors and idle loop
template <>
void init_cpu_for_idle<m68000_device, 0x0200>(address_space &space) {
	// 68000 reads initial SP from 0x0000-0x0003 (big-endian)
	space.write_byte(0x0000, 0x00);
	space.write_byte(0x0001, 0x00);
	space.write_byte(0x0002, 0xFF);
	space.write_byte(0x0003, 0xF0); // SP = 0x0000FFF0

	// 68000 reads initial PC from 0x0004-0x0007 (big-endian)
	space.write_byte(0x0004, 0x00);
	space.write_byte(0x0005, 0x00);
	space.write_byte(0x0006, 0x02);
	space.write_byte(0x0007, 0x00); // PC = 0x00000200

	// Idle loop: BRA.S -2 (branch to self)
	space.write_byte(0x0200, 0x60); // BRA.S opcode
	space.write_byte(0x0201, 0xfe); // Displacement -2
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::machine_reset() {
	init_screen();

	address_space &space = m_maincpu->space(AS_PROGRAM);

	// Call CPU-specific initialization
	init_cpu_for_idle<CPU_TYPE, LOAD_ADDR>(space);
}

// Quickload callback: Load binary program into memory
// Called when user specifies -quik program.bin on command line.
// Loads raw binary at LOAD_ADDR and prepares system for execution.
template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
std::pair<std::error_condition, std::string>
sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::quickload_cb(
    snapshot_image_device &image) {
	uint32_t size = image.length();
	uint32_t vram_addr = get_vram_addr();

	// Validate program fits between LOAD_ADDR and video RAM
	if (size > vram_addr - LOAD_ADDR)
		return std::make_pair(image_error::INVALIDLENGTH, "Program too large");

	// Read into temporary buffer (can't read directly to fragmented address space)
	std::vector<uint8_t> program(size);
	if (image.fread(&program[0], size) != size)
		return std::make_pair(image_error::UNSPECIFIED,
		                      "Failed to read program file");

	// Write program to memory byte-by-byte (handles endianness/bus width)
	address_space &space = m_maincpu->space(AS_PROGRAM);
	for (uint32_t i = 0; i < size; i++)
		space.write_byte(LOAD_ADDR + i, program[i]);

	// Clear screen to remove boot message
	init_screen();

	return std::make_pair(std::error_condition(), std::string()); // Success
}

template <typename CPU_TYPE, uint32_t LOAD_ADDR, uint32_t CPU_SPEED,
          uint32_t VRAM_ADDR>
template <typename DEVICE_TYPE>
void sbc_state<CPU_TYPE, LOAD_ADDR, CPU_SPEED, VRAM_ADDR>::sbc(
    machine_config &config, DEVICE_TYPE const &cpu_type) {
	cpu_type(config, m_maincpu, CPU_SPEED);
	m_maincpu->set_addrmap(AS_PROGRAM, &sbc_state::mem_map);

	SCREEN(config, "screen", SCREEN_TYPE_RASTER);

	MC6847(config, m_vdg, 4.433619_MHz_XTAL, true); // PAL mode
	m_vdg->set_screen("screen");
	m_vdg->fsync_wr_callback().set_inputline(m_maincpu, INPUT_LINE_NMI);
	m_vdg->input_callback().set(FUNC(sbc_state::vdg_videoram_r));

	QUICKLOAD(config, "quickload", "bin")
	    .set_load_callback(FUNC(sbc_state::quickload_cb));
}

} // anonymous namespace

// Macro to define a complete SBC variant in a single line.
//
// This macro generates:
// 1. A derived class (sbc_<name>_state) inheriting from sbc_state template
// 2. ROM definition (empty for these systems)
// 3. MAME machine registration (makes it available via command line)
//
// Parameters:
//   cpu_class: CPU device class for template instantiation (e.g., m6502_device)
//   cpu_type: MAME device type macro for machine_config (e.g., M6502)
//   short_name: Machine name suffix, creates "sbc<short_name>" (e.g., 6502 → sbc6502)
//   display_name: Human-readable name for UI (e.g., "MOS 6502")
//   ...: Optional template parameter overrides (load_addr, cpu_speed, vram_addr)
//        Example: DEFINE_SBC(..., ..., ..., ..., 0x1000, 8000000) sets LOAD_ADDR=0x1000, CPU_SPEED=8MHz
#define DEFINE_SBC(cpu_class, cpu_type, short_name, display_name, ...)         \
	namespace {                                                                \
	class sbc_##short_name##_state                                             \
	    : public sbc_state<cpu_class, ##__VA_ARGS__> {                         \
	  public:                                                                  \
		using sbc_state<cpu_class, ##__VA_ARGS__>::sbc_state;                  \
		void machine_config(machine_config &config) { sbc(config, cpu_type); } \
	};                                                                         \
	}                                                                          \
	ROM_START(sbc##short_name)                                                 \
	ROM_END                                                                    \
	COMP(2025, sbc##short_name, 0, 0, machine_config, 0,                       \
	     sbc_##short_name##_state, empty_init, "MAME",                         \
	     "Single Board Computer - " display_name, MACHINE_NO_SOUND_HW)

// Define SBC variants for major CPU architectures.
// Each line creates a complete emulated machine accessible via command line.
// CPUs with init_cpu_for_idle specializations (6502, Z80, 68000) are fully functional.
// CPUs without specializations may require additional boot code for proper initialization.

// Original 6 CPUs
DEFINE_SBC(m6502_device, M6502, 6502, "MOS 6502")
DEFINE_SBC(z80_device, Z80, z80, "Zilog Z80")
DEFINE_SBC(m68000_device, M68000, 68000, "Motorola 68000")
DEFINE_SBC(arm7_cpu_device, ARM7, arm7, "ARM7")
DEFINE_SBC(i386_device, I386, i386, "Intel 80386")
DEFINE_SBC(r3000a_device, R3000A, mips, "MIPS R3000A")

// Intel/AMD x86 Family (17 new)
DEFINE_SBC(i8086_cpu_device, I8086, i8086, "Intel 8086")
DEFINE_SBC(i8088_cpu_device, I8088, i8088, "Intel 8088")
DEFINE_SBC(i80186_cpu_device, I80186, i80186, "Intel 80186")
DEFINE_SBC(i80188_cpu_device, I80188, i80188, "Intel 80188")
DEFINE_SBC(i80286_cpu_device, I80286, i80286, "Intel 80286")
DEFINE_SBC(i386sx_device, I386SX, i386sx, "Intel 386SX")
DEFINE_SBC(i486_device, I486, i486, "Intel 486")
DEFINE_SBC(i486dx4_device, I486DX4, i486dx4, "Intel 486DX4")
DEFINE_SBC(pentium_device, PENTIUM, pentium, "Intel Pentium")
DEFINE_SBC(pentium_mmx_device, PENTIUM_MMX, pentmmx, "Intel Pentium MMX")
DEFINE_SBC(pentium_pro_device, PENTIUM_PRO, pentpro, "Intel Pentium Pro")
DEFINE_SBC(pentium2_device, PENTIUM2, pent2, "Intel Pentium II")
DEFINE_SBC(pentium3_device, PENTIUM3, pent3, "Intel Pentium III")
DEFINE_SBC(pentium4_device, PENTIUM4, pent4, "Intel Pentium 4")
DEFINE_SBC(mediagx_device, MEDIAGX, mediagx, "Cyrix MediaGX")
DEFINE_SBC(am186em_device, AM186EM, am186em, "AMD Am186EM")
DEFINE_SBC(athlonxp_device, ATHLONXP, athlonxp, "AMD Athlon XP")

// 6502 Variants (15 new)
DEFINE_SBC(m6510_device, M6510, m6510, "MOS 6510 (C64)")
DEFINE_SBC(r65c02_device, R65C02, r65c02, "WDC 65C02")
DEFINE_SBC(w65c02s_device, W65C02S, w65c02s, "WDC W65C02S")
DEFINE_SBC(m65ce02_device, M65CE02, m65ce02, "CSG 65CE02")
DEFINE_SBC(g65sc02_device, G65SC02, g65sc02, "Rockwell G65SC02")
DEFINE_SBC(m4510_device, M4510, m4510, "CSG 4510 (C65)")
DEFINE_SBC(m8502_device, M8502, m8502, "CSG 8502 (C128)")
DEFINE_SBC(rp2a03_device, RP2A03, rp2a03, "Ricoh RP2A03 (NES)")
DEFINE_SBC(m37450_device, M37450, m37450, "Mitsubishi M37450")
DEFINE_SBC(st2204_device, ST2204, st2204, "Sitronix ST2204")
DEFINE_SBC(xavix_device, XAVIX, xavix, "SSD XaviX")
DEFINE_SBC(deco16_device, DECO16, deco16, "Data East DECO16")

// ARM Variants (8 new)
DEFINE_SBC(arm_cpu_device, ARM, arm, "ARM")
DEFINE_SBC(arm9_cpu_device, ARM9, arm9, "ARM9")
DEFINE_SBC(arm920t_cpu_device, ARM920T, arm920t, "ARM920T")
DEFINE_SBC(arm946es_cpu_device, ARM946ES, arm946es, "ARM946ES")
DEFINE_SBC(arm11_cpu_device, ARM11, arm11, "ARM11")
DEFINE_SBC(arm1176jzf_s_cpu_device, ARM1176JZF_S, arm1176, "ARM1176JZF-S")
DEFINE_SBC(pxa255_cpu_device, PXA255, pxa255, "Intel XScale PXA255")
DEFINE_SBC(sa1110_cpu_device, SA1110, sa1110, "StrongARM SA-1110")

// Other Important CPUs (4 new)
DEFINE_SBC(m6800_cpu_device, M6800, m6800, "Motorola 6800")
DEFINE_SBC(mc6809e_device, MC6809E, mc6809e, "Motorola 6809E")
DEFINE_SBC(h6280_device, H6280, h6280, "Hudson H6280")
DEFINE_SBC(sh4_device, SH4, sh4, "Hitachi SuperH-4")

// ============================================================================
// ADDITIONAL CPU DEFINITIONS - Comprehensive collection of all MAME CPUs
// ============================================================================

// ----------------------------------------------------------------------------
// Classic 8-bit CPUs - Additional variants
// ----------------------------------------------------------------------------

// Intel 8080/8085 family
DEFINE_SBC(i8080_cpu_device, I8080, i8080, "Intel 8080")
DEFINE_SBC(i8080a_cpu_device, I8080A, i8080a, "Intel 8080A")

// MOS 6502 family - Additional variants
DEFINE_SBC(m6503_device, M6503, m6503, "MOS 6503")
DEFINE_SBC(m6504_device, M6504, m6504, "MOS 6504")
DEFINE_SBC(m6509_device, M6509, m6509, "MOS 6509")
DEFINE_SBC(m6510t_device, M6510T, m6510t, "MOS 6510T")
DEFINE_SBC(m7501_device, M7501, m7501, "MOS 7501")
// DEFINE_SBC(m8500_device, M8500, m8500, "CSG 8500") // M8500 does not exist - CSG 8502 is M8502
// DEFINE_SBC(n2a03_device, N2A03, n2a03, "Ricoh N2A03 (NES)") // N2A03 doesn't exist, use RP2A03
// DEFINE_SBC(rp2a07_device, RP2A07, rp2a07, "Ricoh RP2A07 (PAL NES)") // RP2A07 doesn't exist
DEFINE_SBC(rp2a03g_device, RP2A03G, rp2a03g, "Ricoh RP2A03G (PAL NES)")
// M5074X doesn't exist - using M50740 and M50741 instead
DEFINE_SBC(m50740_device, M50740, m50740, "Mitsubishi M50740")
DEFINE_SBC(m50741_device, M50741, m50741, "Mitsubishi M50741")
DEFINE_SBC(st2205u_device, ST2205U, st2205u, "Sitronix ST2205U")
DEFINE_SBC(st2302u_device, ST2302U, st2302u, "Sitronix ST2302U")
DEFINE_SBC(xavix2_device, XAVIX2, xavix2, "SSD XaviX 2")
DEFINE_SBC(xavix2000_device, XAVIX2000, xavix2000, "SSD XaviX 2000")

// Motorola 6800 family
DEFINE_SBC(m6801_cpu_device, M6801, m6801, "Motorola 6801")
DEFINE_SBC(m6802_cpu_device, M6802, m6802, "Motorola 6802")
DEFINE_SBC(m6803_cpu_device, M6803, m6803, "Motorola 6803")
DEFINE_SBC(m6808_cpu_device, M6808, m6808, "Motorola 6808")
// HD6301 generic doesn't exist - using specific variants
DEFINE_SBC(hd6301v1_cpu_device, HD6301V1, hd6301v1, "Hitachi HD6301V1")
DEFINE_SBC(hd6301x0_cpu_device, HD6301X0, hd6301x0, "Hitachi HD6301X0")
DEFINE_SBC(hd6301y0_cpu_device, HD6301Y0, hd6301y0, "Hitachi HD6301Y0")
DEFINE_SBC(hd6303r_cpu_device, HD6303R, hd6303r, "Hitachi HD6303R")
DEFINE_SBC(hd6303y_cpu_device, HD6303Y, hd6303y, "Hitachi HD6303Y")
DEFINE_SBC(nsc8105_cpu_device, NSC8105, nsc8105, "National NSC8105")

// Motorola 6809 family
DEFINE_SBC(m6809_device, M6809, m6809, "Motorola 6809")
DEFINE_SBC(hd6309_device, HD6309, hd6309, "Hitachi HD6309")
DEFINE_SBC(konami_cpu_device, KONAMI, konami, "Konami Custom 6809")

// Zilog Z80 family
// z80_cpu_device doesn't exist, it's just z80_device - but that's already defined at line 849
// DEFINE_SBC(z80_cpu_device, Z80, z80cpu, "Zilog Z80 CPU") // Duplicate, Z80 already defined
DEFINE_SBC(nsc800_device, NSC800, nsc800, "National NSC800")
// DEFINE_SBC(z180_device, Z180, z180, "Zilog Z180") // Z180 device type doesn't exist
DEFINE_SBC(hd64180rp_device, HD64180RP, hd64180rp, "Hitachi HD64180RP")
// DEFINE_SBC(z80daisy_device, Z80DAISY, z80daisy, "Z80 Daisy Chain") // z80daisy is not a CPU

// ----------------------------------------------------------------------------
// 16-bit CPUs
// ----------------------------------------------------------------------------

// Intel 8086 family - already covered in existing entries

// NEC V-series
DEFINE_SBC(v20_device, V20, v20, "NEC V20")
DEFINE_SBC(v30_device, V30, v30, "NEC V30")
DEFINE_SBC(v33_device, V33, v33, "NEC V33")
DEFINE_SBC(v33a_device, V33A, v33a, "NEC V33A")
DEFINE_SBC(v40_device, V40, v40, "NEC V40")
DEFINE_SBC(v50_device, V50, v50, "NEC V50")
DEFINE_SBC(v53_device, V53, v53, "NEC V53")
DEFINE_SBC(v53a_device, V53A, v53a, "NEC V53A")

// Zilog Z8000 family
DEFINE_SBC(z8001_device, Z8001, z8001, "Zilog Z8001")
DEFINE_SBC(z8002_device, Z8002, z8002, "Zilog Z8002")

// WDC 65816
DEFINE_SBC(g65816_device, G65816, g65816, "WDC 65C816")
// W65C816S doesn't exist - G65816 is the generic 65816
DEFINE_SBC(_5a22_device, _5A22, 5a22, "Ricoh 5A22 (SNES)")

// Mitsubishi M37710
DEFINE_SBC(m37710s4_device, M37710S4, m37710s4, "Mitsubishi M37710S4")

// ----------------------------------------------------------------------------
// 32-bit RISC CPUs
// ----------------------------------------------------------------------------

// ARM family - some already covered
DEFINE_SBC(arm7500_cpu_device, ARM7500, arm7500, "ARM7500")
DEFINE_SBC(arm710a_cpu_device, ARM710A, arm710a, "ARM710a")
DEFINE_SBC(arm710t_cpu_device, ARM710T, arm710t, "ARM710T")
DEFINE_SBC(arm7_be_cpu_device, ARM7_BE, arm7be, "ARM7 Big-Endian")
// Big-endian variants for ARM9/920T/946ES/11/1176 don't exist as separate device types
// DEFINE_SBC(arm9_be_cpu_device, ARM9_BE, arm9be, "ARM9 Big-Endian")
// DEFINE_SBC(arm920t_be_cpu_device, ARM920T_BE, arm920tbe, "ARM920T Big-Endian")
// DEFINE_SBC(arm946es_be_cpu_device, ARM946ES_BE, arm946esbe, "ARM946ES Big-Endian")
// DEFINE_SBC(arm11_be_cpu_device, ARM11_BE, arm11be, "ARM11 Big-Endian")
// DEFINE_SBC(arm1176jzf_s_be_cpu_device, ARM1176JZF_S_BE, arm1176be, "ARM1176JZF-S Big-Endian")
DEFINE_SBC(pxa250_cpu_device, PXA250, pxa250, "Intel XScale PXA250")
DEFINE_SBC(pxa270_cpu_device, PXA270, pxa270, "Intel XScale PXA270")
DEFINE_SBC(sa1100_cpu_device, SA1100, sa1100, "StrongARM SA-1100")

// MIPS I family
DEFINE_SBC(r2000_device, R2000, r2000, "MIPS R2000")
DEFINE_SBC(r2000a_device, R2000A, r2000a, "MIPS R2000A")
DEFINE_SBC(r3000_device, R3000, r3000, "MIPS R3000")
DEFINE_SBC(r3041_device, R3041, r3041, "MIPS R3041")
DEFINE_SBC(r3051_device, R3051, r3051, "MIPS R3051")
DEFINE_SBC(r3052_device, R3052, r3052, "MIPS R3052")
DEFINE_SBC(r3052e_device, R3052E, r3052e, "MIPS R3052E")
DEFINE_SBC(r3071_device, R3071, r3071, "MIPS R3071")
DEFINE_SBC(r3081_device, R3081, r3081, "MIPS R3081")
DEFINE_SBC(iop_device, SONYPS2_IOP, ps2iop, "Sony PlayStation 2 IOP")

// MIPS III/IV family
DEFINE_SBC(r4000be_device, R4000BE, r4000be, "MIPS R4000 Big-Endian")
DEFINE_SBC(r4000le_device, R4000LE, r4000le, "MIPS R4000 Little-Endian")
DEFINE_SBC(vr4300be_device, VR4300BE, vr4300be, "NEC VR4300 Big-Endian")
DEFINE_SBC(vr4300le_device, VR4300LE, vr4300le, "NEC VR4300 Little-Endian")
DEFINE_SBC(r4600be_device, R4600BE, r4600be, "MIPS R4600 Big-Endian")
DEFINE_SBC(r4600le_device, R4600LE, r4600le, "MIPS R4600 Little-Endian")
DEFINE_SBC(r4650be_device, R4650BE, r4650be, "MIPS R4650 Big-Endian")
DEFINE_SBC(r4650le_device, R4650LE, r4650le, "MIPS R4650 Little-Endian")
DEFINE_SBC(r4700be_device, R4700BE, r4700be, "MIPS R4700 Big-Endian")
DEFINE_SBC(r4700le_device, R4700LE, r4700le, "MIPS R4700 Little-Endian")
DEFINE_SBC(tx4925be_device, TX4925BE, tx4925be, "Toshiba TX4925 Big-Endian")
DEFINE_SBC(tx4925le_device, TX4925LE, tx4925le, "Toshiba TX4925 Little-Endian")
DEFINE_SBC(r5000be_device, R5000BE, r5000be, "MIPS R5000 Big-Endian")
DEFINE_SBC(r5000le_device, R5000LE, r5000le, "MIPS R5000 Little-Endian")
DEFINE_SBC(vr5500be_device, VR5500BE, vr5500be, "NEC VR5500 Big-Endian")
DEFINE_SBC(vr5500le_device, VR5500LE, vr5500le, "NEC VR5500 Little-Endian")
DEFINE_SBC(rm7000be_device, RM7000BE, rm7000be, "QED RM7000 Big-Endian")
DEFINE_SBC(rm7000le_device, RM7000LE, rm7000le, "QED RM7000 Little-Endian")
DEFINE_SBC(r5900be_device, R5900BE, r5900be, "Toshiba R5900 Big-Endian (PS2 EE)")
DEFINE_SBC(r5900le_device, R5900LE, r5900le, "Toshiba R5900 Little-Endian (PS2 EE)")

// PowerPC family
DEFINE_SBC(ppc403ga_device, PPC403GA, ppc403ga, "PowerPC 403GA")
DEFINE_SBC(ppc403gcx_device, PPC403GCX, ppc403gcx, "PowerPC 403GCX")
DEFINE_SBC(ppc405gp_device, PPC405GP, ppc405gp, "PowerPC 405GP")
DEFINE_SBC(ppc601_device, PPC601, ppc601, "PowerPC 601")
DEFINE_SBC(ppc602_device, PPC602, ppc602, "PowerPC 602")
DEFINE_SBC(ppc603_device, PPC603, ppc603, "PowerPC 603")
DEFINE_SBC(ppc603e_device, PPC603E, ppc603e, "PowerPC 603e")
DEFINE_SBC(ppc603r_device, PPC603R, ppc603r, "PowerPC 603r")
DEFINE_SBC(ppc604_device, PPC604, ppc604, "PowerPC 604")
DEFINE_SBC(mpc8240_device, MPC8240, mpc8240, "Motorola MPC8240")

// SPARC family
// DEFINE_SBC(mb86901_device, MB86901, mb86901, "Fujitsu MB86901 (SPARC)")
DEFINE_SBC(mb86930_device, MB86930, mb86930, "Fujitsu MB86930 (SPARClite)")
// DEFINE_SBC(cy7c601_device, CY7C601, cy7c601, "Cypress CY7C601 (SPARClite)")
// DEFINE_SBC(tms390s10_device, TMS390S10, tms390s10, "TI TMS390S10 (SPARC)")

// DEC Alpha
// DEFINE_SBC(alpha_device, ALPHA_21064, alpha21064, "DEC Alpha 21064")
// DEFINE_SBC(alpha_21164_device, ALPHA_21164, alpha21164, "DEC Alpha 21164")
// DEFINE_SBC(alpha_21264_device, ALPHA_21264, alpha21264, "DEC Alpha 21264")

// Intel i860
DEFINE_SBC(i860_cpu_device, I860, i860, "Intel i860")

// Intel i960
DEFINE_SBC(i80960ka_device, I80960KA, i80960ka, "Intel i960KA")
DEFINE_SBC(i80960kb_device, I80960KB, i80960kb, "Intel i960KB")

// Clipper
// DEFINE_SBC(clipper_c100_device, CLIPPER_C100, clipperc100, "Fairchild Clipper C100") // BROKEN: validation fails
// DEFINE_SBC(clipper_c300_device, CLIPPER_C300, clipperc300, "Fairchild Clipper C300") // BROKEN: validation fails
// DEFINE_SBC(clipper_c400_device, CLIPPER_C400, clipperc400, "Fairchild Clipper C400") // BROKEN: validation fails

// ROMP
DEFINE_SBC(romp_device, ROMP, romp, "IBM ROMP")

// AM29000
// DEFINE_SBC(am29000_cpu_device, AM29000, am29000, "AMD Am29000") // Conflicts with clipper.h

// ----------------------------------------------------------------------------
// 32-bit CISC CPUs
// ----------------------------------------------------------------------------

// Intel x86 32-bit - already covered

// Motorola 68000 family
DEFINE_SBC(m68008_device, M68008, m68008, "Motorola 68008")
DEFINE_SBC(m68010_device, M68010, m68010, "Motorola 68010")
DEFINE_SBC(m68ec020_device, M68EC020, m68ec020, "Motorola 68EC020")
DEFINE_SBC(m68020_device, M68020, m68020, "Motorola 68020")
DEFINE_SBC(m68020fpu_device, M68020FPU, m68020fpu, "Motorola 68020 with FPU")
DEFINE_SBC(m68ec030_device, M68EC030, m68ec030, "Motorola 68EC030")
DEFINE_SBC(m68030_device, M68030, m68030, "Motorola 68030")
DEFINE_SBC(m68ec040_device, M68EC040, m68ec040, "Motorola 68EC040")
DEFINE_SBC(m68lc040_device, M68LC040, m68lc040, "Motorola 68LC040")
DEFINE_SBC(m68040_device, M68040, m68040, "Motorola 68040")
// DEFINE_SBC(m68060_device, M68060, m68060, "Motorola 68060")
// DEFINE_SBC(scc68070_device, SCC68070, scc68070, "Philips SCC68070 (CD-i)")
// DEFINE_SBC(fscpu32_device, FSCPU32, fscpu32, "Freescale CPU32")
DEFINE_SBC(mcf5206e_device, MCF5206E, mcf5206e, "Freescale MCF5206E (ColdFire)")

// National Semiconductor NS32000
DEFINE_SBC(ns32008_device, NS32008, ns32008, "NS32008")
DEFINE_SBC(ns32016_device, NS32016, ns32016, "NS32016")
DEFINE_SBC(ns32032_device, NS32032, ns32032, "NS32032")
DEFINE_SBC(ns32332_device, NS32332, ns32332, "NS32332")
// DEFINE_SBC(ns32382_device, NS32382, ns32382, "NS32382")

// DEC VAX
// DEFINE_SBC(vax_device, VAX, vax, "DEC VAX")

// AT&T WE32100
DEFINE_SBC(we32100_device, WE32100, we32100, "AT&T WE32100")

// NEC V60/V70
DEFINE_SBC(v60_device, V60, v60, "NEC V60")
DEFINE_SBC(v70_device, V70, v70, "NEC V70")

// Motorola 88000
DEFINE_SBC(mc88100_device, MC88100, mc88100, "Motorola MC88100")

// ----------------------------------------------------------------------------
// DSP Processors
// ----------------------------------------------------------------------------

// Analog Devices ADSP-21xx
// DEFINE_SBC(adsp2100_device, ADSP2100, adsp2100, "Analog Devices ADSP-2100") // BROKEN: validation fails
// DEFINE_SBC(adsp2101_device, ADSP2101, adsp2101, "Analog Devices ADSP-2101") // BROKEN: validation fails
// DEFINE_SBC(adsp2102_device, ADSP2102, adsp2102, "Analog Devices ADSP-2102")
// DEFINE_SBC(adsp2103_device, ADSP2103, adsp2103, "Analog Devices ADSP-2103")
// DEFINE_SBC(adsp2104_device, ADSP2104, adsp2104, "Analog Devices ADSP-2104") // BROKEN: validation fails
// DEFINE_SBC(adsp2105_device, ADSP2105, adsp2105, "Analog Devices ADSP-2105") // BROKEN: validation fails
// DEFINE_SBC(adsp2115_device, ADSP2115, adsp2115, "Analog Devices ADSP-2115") // BROKEN: validation fails
// DEFINE_SBC(adsp2181_device, ADSP2181, adsp2181, "Analog Devices ADSP-2181") // BROKEN: validation fails

// Analog Devices SHARC
DEFINE_SBC(adsp21062_device, ADSP21062, adsp21062, "Analog Devices ADSP-21062 (SHARC)")

// WE DSP16
DEFINE_SBC(dsp16_device, DSP16, dsp16, "AT&T DSP16")

// WE DSP32C
DEFINE_SBC(dsp32c_device, DSP32C, dsp32c, "AT&T DSP32C")

// Motorola DSP56000 family
DEFINE_SBC(dsp56001_device, DSP56001, dsp56001, "Motorola DSP56001")
// DEFINE_SBC(dsp56002_device, DSP56002, dsp56002, "Motorola DSP56002")
// DEFINE_SBC(dsp56156_device, DSP56156, dsp56156, "Motorola DSP56156")
DEFINE_SBC(dsp56303_device, DSP56303, dsp56303, "Motorola DSP56303")

// Texas Instruments TMS320 family
DEFINE_SBC(tms32010_device, TMS32010, tms32010, "TI TMS32010")
DEFINE_SBC(tms32015_device, TMS32015, tms32015, "TI TMS32015")
DEFINE_SBC(tms32025_device, TMS32025, tms32025, "TI TMS32025")
DEFINE_SBC(tms32026_device, TMS32026, tms32026, "TI TMS32026")
DEFINE_SBC(tms32031_device, TMS32031, tms32031, "TI TMS32031")
DEFINE_SBC(tms32051_device, TMS32051, tms32051, "TI TMS32051")
DEFINE_SBC(tms32082_pp_device, TMS32082_PP, tms32082pp, "TI TMS32082 PP")
DEFINE_SBC(tms32082_mp_device, TMS32082_MP, tms32082mp, "TI TMS32082 MP")

// NEC uPD77xx
DEFINE_SBC(upd7725_device, UPD7725, upd7725, "NEC uPD7725")
// DEFINE_SBC(upd7720_device, UPD7720, upd7720, "NEC uPD7720")
DEFINE_SBC(upd7810_device, UPD7810, upd7810, "NEC uPD7810")

// ----------------------------------------------------------------------------
// Microcontrollers - 8051 family
// ----------------------------------------------------------------------------

DEFINE_SBC(i8051_device, I8051, i8051, "Intel 8051")
DEFINE_SBC(i8052_device, I8052, i8052, "Intel 8052")
DEFINE_SBC(i80c51_device, I80C51, i80c51, "Intel 80C51")
DEFINE_SBC(i80c52_device, I80C52, i80c52, "Intel 80C52")
// DEFINE_SBC(at89c4051_device, AT89C4051, at89c4051, "Atmel AT89C4051") // BROKEN: validation fails
DEFINE_SBC(ds5002fp_device, DS5002FP, ds5002fp, "Dallas DS5002FP")

// ----------------------------------------------------------------------------
// Microcontrollers - PIC
// ----------------------------------------------------------------------------

DEFINE_SBC(pic16c54_device, PIC16C54, pic16c54, "Microchip PIC16C54")
DEFINE_SBC(pic16c55_device, PIC16C55, pic16c55, "Microchip PIC16C55")
DEFINE_SBC(pic16c56_device, PIC16C56, pic16c56, "Microchip PIC16C56")
DEFINE_SBC(pic16c57_device, PIC16C57, pic16c57, "Microchip PIC16C57")
DEFINE_SBC(pic16c58_device, PIC16C58, pic16c58, "Microchip PIC16C58")
// DEFINE_SBC(pic16c62x_device, PIC16C62X, pic16c62x, "Microchip PIC16C62x")
DEFINE_SBC(pic17c43_device, PIC17C43, pic17c43, "Microchip PIC17C43")
DEFINE_SBC(pic17c44_device, PIC17C44, pic17c44, "Microchip PIC17C44")

// ----------------------------------------------------------------------------
// Microcontrollers - AVR
// ----------------------------------------------------------------------------

// DEFINE_SBC(atmega88_device, ATMEGA88, atmega88, "Atmel ATmega88") // BROKEN: validation fails
// DEFINE_SBC(atmega644_device, ATMEGA644, atmega644, "Atmel ATmega644") // BROKEN: validation fails
// DEFINE_SBC(atmega1280_device, ATMEGA1280, atmega1280, "Atmel ATmega1280") // BROKEN: validation fails
// DEFINE_SBC(atmega2560_device, ATMEGA2560, atmega2560, "Atmel ATmega2560") // BROKEN: validation fails

// ----------------------------------------------------------------------------
// Microcontrollers - Motorola 68HC
// ----------------------------------------------------------------------------

DEFINE_SBC(m68hc05c4_device, M68HC05C4, m68hc05c4, "Motorola 68HC05C4")
DEFINE_SBC(m68hc05eg_device, M68HC05EG, m68hc05eg, "Motorola 68HC05EG")
// DEFINE_SBC(m68hc11a1_device, M68HC11A1, m68hc11a1, "Motorola 68HC11A1")
// DEFINE_SBC(m68hc11d0_device, M68HC11D0, m68hc11d0, "Motorola 68HC11D0")
// DEFINE_SBC(m68hc11f1_device, M68HC11F1, m68hc11f1, "Motorola 68HC11F1")
// DEFINE_SBC(m68hc11k1_device, M68HC11K1, m68hc11k1, "Motorola 68HC11K1")
// DEFINE_SBC(m68hc11m0_device, M68HC11M0, m68hc11m0, "Motorola 68HC11M0")

// ----------------------------------------------------------------------------
// Microcontrollers - Intel MCS-48
// ----------------------------------------------------------------------------

DEFINE_SBC(i8021_device, I8021, i8021, "Intel 8021")
DEFINE_SBC(i8022_device, I8022, i8022, "Intel 8022")
DEFINE_SBC(i8035_device, I8035, i8035, "Intel 8035")
DEFINE_SBC(i8039_device, I8039, i8039, "Intel 8039")
DEFINE_SBC(i8040_device, I8040, i8040, "Intel 8040")
DEFINE_SBC(i8048_device, I8048, i8048, "Intel 8048")
DEFINE_SBC(i8648_device, I8648, i8648, "Intel 8648")
DEFINE_SBC(i8748_device, I8748, i8748, "Intel 8748")
// DEFINE_SBC(i8041_device, I8041, i8041, "Intel 8041")
// DEFINE_SBC(i8741_device, I8741, i8741, "Intel 8741")
DEFINE_SBC(i8042_device, I8042, i8042, "Intel 8042")
// DEFINE_SBC(i8242_device, I8242, i8242, "Intel 8242")
DEFINE_SBC(i8742_device, I8742, i8742, "Intel 8742")
DEFINE_SBC(mb8884_device, MB8884, mb8884, "Fujitsu MB8884")
// DEFINE_SBC(n7751_device, N7751, n7751, "NEC N7751 (Audio CPU)")

// ----------------------------------------------------------------------------
// Microcontrollers - Intel MCS-96
// ----------------------------------------------------------------------------

// DEFINE_SBC(i8096_device, I8096, i8096, "Intel 8096")
// DEFINE_SBC(i8395_device, I8395, i8395, "Intel 8395")

// ----------------------------------------------------------------------------
// Microcontrollers - Hitachi H8
// ----------------------------------------------------------------------------

// DEFINE_SBC(h8_device, H8, h8, "Hitachi H8")
DEFINE_SBC(h8325_device, H8325, h8325, "Hitachi H8/325")
// DEFINE_SBC(h8520_device, H8520, h8520, "Hitachi H8/520")
// DEFINE_SBC(h8h_device, H8H, h8h, "Hitachi H8H")
// DEFINE_SBC(h8s2000_device, H8S2000, h8s2000, "Hitachi H8S/2000")
// DEFINE_SBC(h8s2600_device, H8S2600, h8s2600, "Hitachi H8S/2600")
DEFINE_SBC(gt913_device, GT913, gt913, "Hitachi GT913")

// ----------------------------------------------------------------------------
// Microcontrollers - Hitachi/Renesas SuperH
// ----------------------------------------------------------------------------

// DEFINE_SBC(sh1_device, SH1, sh1, "Hitachi SH-1")
// DEFINE_SBC(sh2_device, SH2, sh2, "Hitachi SH-2")
// DEFINE_SBC(sh2a_device, SH2A, sh2a, "Renesas SH-2A")
DEFINE_SBC(sh3_device, SH3, sh3, "Hitachi SH-3")
// DEFINE_SBC(sh3be_device, SH3BE, sh3be, "Hitachi SH-3 Big-Endian")
// DEFINE_SBC(sh4be_device, SH4BE, sh4be, "Hitachi SH-4 Big-Endian")

// ----------------------------------------------------------------------------
// Microcontrollers - NEC uPD7xxx
// ----------------------------------------------------------------------------

DEFINE_SBC(upd7801_device, UPD7801, upd7801, "NEC uPD7801")
DEFINE_SBC(upd7807_device, UPD7807, upd7807, "NEC uPD7807")
DEFINE_SBC(upd78c05_device, UPD78C05, upd78c05, "NEC uPD78C05")
DEFINE_SBC(upd78c06_device, UPD78C06, upd78c06, "NEC uPD78C06")
DEFINE_SBC(upd78c10_device, UPD78C10, upd78c10, "NEC uPD78C10")
DEFINE_SBC(upd78c11_device, UPD78C11, upd78c11, "NEC uPD78C11")
DEFINE_SBC(upd78c14_device, UPD78C14, upd78c14, "NEC uPD78C14")

// ----------------------------------------------------------------------------
// Microcontrollers - Toshiba TLCS
// ----------------------------------------------------------------------------

DEFINE_SBC(tmp90840_device, TMP90840, tmp90840, "Toshiba TMP90840 (TLCS-90)")
DEFINE_SBC(tmp90841_device, TMP90841, tmp90841, "Toshiba TMP90841 (TLCS-90)")
DEFINE_SBC(tmp91640_device, TMP91640, tmp91640, "Toshiba TMP91640 (TLCS-900)")
DEFINE_SBC(tmp91641_device, TMP91641, tmp91641, "Toshiba TMP91641 (TLCS-900)")

// ----------------------------------------------------------------------------
// Microcontrollers - Fujitsu F2MC
// ----------------------------------------------------------------------------

// DEFINE_SBC(mb9061x_device, MB9061X, mb9061x, "Fujitsu MB9061x (F2MC-16)")

// ----------------------------------------------------------------------------
// Microcontrollers - Epson S1C63
// ----------------------------------------------------------------------------

// DEFINE_SBC(s1c63_device, S1C63, s1c63, "Epson S1C63")

// ----------------------------------------------------------------------------
// 4-bit Microcontrollers
// ----------------------------------------------------------------------------

// Intel 4004/4040
// DEFINE_SBC(i4004_device, I4004, i4004, "Intel 4004")
// DEFINE_SBC(i4040_device, I4040, i4040, "Intel 4040")

// National Semiconductor COP400
// DEFINE_SBC(cop410_device, COP410, cop410, "National COP410")
// DEFINE_SBC(cop411_device, COP411, cop411, "National COP411")
// DEFINE_SBC(cop420_device, COP420, cop420, "National COP420")
// DEFINE_SBC(cop421_device, COP421, cop421, "National COP421")
// DEFINE_SBC(cop422_device, COP422, cop422, "National COP422")
// DEFINE_SBC(cop444_device, COP444, cop444, "National COP444")
// DEFINE_SBC(cop445_device, COP445, cop445, "National COP445")

// Texas Instruments TMS1000 family
// DEFINE_SBC(tms1000_device, TMS1000, tms1000, "TI TMS1000")
// DEFINE_SBC(tms1070_device, TMS1070, tms1070, "TI TMS1070")
// DEFINE_SBC(tms1100_device, TMS1100, tms1100, "TI TMS1100")
// DEFINE_SBC(tms1200_device, TMS1200, tms1200, "TI TMS1200")
// DEFINE_SBC(tms1300_device, TMS1300, tms1300, "TI TMS1300")
// DEFINE_SBC(tms0980_device, TMS0980, tms0980, "TI TMS0980 (TMS1000 variant)")
// DEFINE_SBC(tmc0270_device, TMC0270, tmc0270, "TI TMC0270")
// DEFINE_SBC(tmc0280_device, TMC0280, tmc0280, "TI TMC0280")
// DEFINE_SBC(tms0270_device, TMS0270, tms0270, "TI TMS0270")
// DEFINE_SBC(tms2100_device, TMS2100, tms2100, "TI TMS2100")
// DEFINE_SBC(tms2400_device, TMS2400, tms2400, "TI TMS2400")
// DEFINE_SBC(tms2600_device, TMS2600, tms2600, "TI TMS2600")

// Hitachi HMCS40/HMCS400
// DEFINE_SBC(hmcs40_device, HMCS40, hmcs40, "Hitachi HMCS40")
// DEFINE_SBC(hmcs42_device, HMCS42, hmcs42, "Hitachi HMCS42")
// DEFINE_SBC(hmcs43_device, HMCS43, hmcs43, "Hitachi HMCS43")
// DEFINE_SBC(hmcs44_device, HMCS44, hmcs44, "Hitachi HMCS44")
// DEFINE_SBC(hmcs45_device, HMCS45, hmcs45, "Hitachi HMCS45")
// DEFINE_SBC(hmcs46_device, HMCS46, hmcs46, "Hitachi HMCS46")
// DEFINE_SBC(hmcs48_device, HMCS48, hmcs48, "Hitachi HMCS48")

// NEC uCOM-4
// DEFINE_SBC(upd553_device, UPD553, upd553, "NEC uPD553 (uCOM-4)")
// DEFINE_SBC(upd557_device, UPD557, upd557, "NEC uPD557 (uCOM-4)")
// DEFINE_SBC(upd650_device, UPD650, upd650, "NEC uPD650 (uCOM-4)")

// Rockwell PPS-4
DEFINE_SBC(pps4_device, PPS4, pps4, "Rockwell PPS-4")

// Sharp SM500/SM510/SM511/SM530
DEFINE_SBC(sm500_device, SM500, sm500, "Sharp SM500")
DEFINE_SBC(sm510_device, SM510, sm510, "Sharp SM510")
DEFINE_SBC(sm511_device, SM511, sm511, "Sharp SM511")
DEFINE_SBC(sm530_device, SM530, sm530, "Sharp SM530")
DEFINE_SBC(sm590_device, SM590, sm590, "Sharp SM590")
DEFINE_SBC(sm5a_device, SM5A, sm5a, "Sharp SM5A")

// ----------------------------------------------------------------------------
// Game/Graphics CPUs
// ----------------------------------------------------------------------------

// Atari Jaguar
// DEFINE_SBC(jaguargpu_device, JAGUARGPU, jaguargpu, "Atari Jaguar GPU")
// DEFINE_SBC(jaguardsp_device, JAGUARDSP, jaguardsp, "Atari Jaguar DSP")

// Nintendo 64 RSP
DEFINE_SBC(rsp_device, RSP, rsp, "Nintendo 64 RSP")

// SNES SuperFX
// DEFINE_SBC(superfx_device, SUPERFX, superfx, "SNES SuperFX GSU-1")

// SNES SPC700
DEFINE_SBC(spc700_device, SPC700, spc700, "Sony SPC700 (SNES Audio)")

// PlayStation
// DEFINE_SBC(psxcpu_device, PSXCPU, psxcpu, "Sony PlayStation CPU")
// DEFINE_SBC(cxd8530bq_device, CXD8530BQ, cxd8530bq, "Sony CXD8530BQ (PS1)")
// DEFINE_SBC(cxd8530cq_device, CXD8530CQ, cxd8530cq, "Sony CXD8530CQ (PS1)")
// DEFINE_SBC(cxd8661r_device, CXD8661R, cxd8661r, "Sony CXD8661R (PS1)")

// Sega Saturn SCU DSP
// DEFINE_SBC(scudsp_device, SCUDSP, scudsp, "Sega Saturn SCU DSP")

// Capcom CPS-A
// DEFINE_SBC(cps1_device, CPS1, cps1, "Capcom CPS-A")

// Konami custom CPUs
// DEFINE_SBC(konamigx_device, KONAMIGX, konamigx, "Konami GX CPU")

// ----------------------------------------------------------------------------
// Vintage/Historic CPUs
// ----------------------------------------------------------------------------

// DEC PDP-1
DEFINE_SBC(pdp1_device, PDP1, pdp1, "DEC PDP-1")

// DEC PDP-8
DEFINE_SBC(pdp8_device, PDP8, pdp8, "DEC PDP-8")

// DEC T-11
DEFINE_SBC(t11_device, T11, t11, "DEC T-11")

// Manchester SSEM (Baby)
// DEFINE_SBC(ssem_device, SSEM, ssem, "Manchester SSEM")

// MIT TX-0
DEFINE_SBC(tx0_64kw_device, TX0_64KW, tx064kw, "MIT TX-0 64KW")
DEFINE_SBC(tx0_8kw_device, TX0_8KW, tx08kw, "MIT TX-0 8KW")

// Xerox Alto II
// DEFINE_SBC(alto2_cpu_device, ALTO2, alto2, "Xerox Alto II") // BROKEN: validation fails

// IBM System/370
// DEFINE_SBC(s370_device, S370, s370, "IBM System/370")

// ----------------------------------------------------------------------------
// Calculator/Terminal CPUs
// ----------------------------------------------------------------------------

// HP Saturn
DEFINE_SBC(saturn_device, SATURN, saturn, "HP Saturn")

// Sharp HCD62121
// DEFINE_SBC(hcd62121_device, HCD62121, hcd62121, "Sharp HCD62121 (Pocket Computer)")

// Sharp SC61860
DEFINE_SBC(sc61860_device, SC61860, sc61860, "Sharp SC61860 (Pocket Computer)")

// Sharp HD61700
// DEFINE_SBC(hd61700_device, HD61700, hd61700, "Sharp HD61700 (Pocket Computer)")

// HP Capricorn
// DEFINE_SBC(capricorn_device, CAPRICORN, capricorn, "HP Capricorn")

// ----------------------------------------------------------------------------
// Miscellaneous CPUs
// ----------------------------------------------------------------------------

// Intel 8008
DEFINE_SBC(i8008_device, I8008, i8008, "Intel 8008")

// Intel 8089
DEFINE_SBC(i8089_device, I8089, i8089, "Intel 8089 I/O Processor")

// Fairchild F8
// DEFINE_SBC(f8_device, F8, f8, "Fairchild F8")

// Signetics 2650
DEFINE_SBC(s2650_device, S2650, s2650, "Signetics 2650")

// National SC/MP
DEFINE_SBC(scmp_device, SCMP, scmp, "National SC/MP")
DEFINE_SBC(ins8060_device, INS8060, ins8060, "National INS8060 SC/MP II")

// Texas Instruments TMS9900
DEFINE_SBC(tms9900_device, TMS9900, tms9900, "TI TMS9900")
// DEFINE_SBC(tms9940_device, TMS9940, tms9940, "TI TMS9940")
DEFINE_SBC(tms9980a_device, TMS9980A, tms9980a, "TI TMS9980A")
// DEFINE_SBC(tms9985_device, TMS9985, tms9985, "TI TMS9985")
// DEFINE_SBC(tms9989_device, TMS9989, tms9989, "TI TMS9989")
DEFINE_SBC(tms9995_device, TMS9995, tms9995, "TI TMS9995")
// DEFINE_SBC(tms99105a_device, TMS99105A, tms99105a, "TI TMS99105A")
// DEFINE_SBC(tms99110a_device, TMS99110A, tms99110a, "TI TMS99110A")

// Texas Instruments TMS7000
DEFINE_SBC(tms7000_device, TMS7000, tms7000, "TI TMS7000")
DEFINE_SBC(tms70c00_device, TMS70C00, tms70c00, "TI TMS70C00")
DEFINE_SBC(tms70c20_device, TMS70C20, tms70c20, "TI TMS70C20")
DEFINE_SBC(tms70c40_device, TMS70C40, tms70c40, "TI TMS70C40")

// Zilog Z8
// DEFINE_SBC(z8_device, Z8, z8, "Zilog Z8")

// RCA COSMAC
DEFINE_SBC(cdp1802_device, CDP1802, cdp1802, "RCA CDP1802 (COSMAC)")
DEFINE_SBC(cdp1805_device, CDP1805, cdp1805, "RCA CDP1805 (COSMAC)")

// GI CP1600
// DEFINE_SBC(cp1600_device, CP1600, cp1600, "GI CP1600 (Intellivision)")

// Acorn RISC Machine
// DEFINE_SBC(arm_cpu_device, ARM_CPU, armcpu, "Acorn RISC Machine")

// MOS 6100
DEFINE_SBC(hd6120_device, HD6120, hd6120, "Harris HD6120 (PDP-8 compatible)")

// Sanyo LC8670
// DEFINE_SBC(lc8670_device, LC8670, lc8670, "Sanyo LC8670")

// Sony NEWS R3000
// DEFINE_SBC(sonymips_device, SONYMIPS, sonymips, "Sony NEWS R3000")

// Western Design Center W65C02S - already covered

// ----------------------------------------------------------------------------
// Additional 6502 variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(m65sc02_device, M65SC02, m65sc02, "MOS 65SC02")
// DEFINE_SBC(m65c02_device, M65C02, m65c02, "MOS 65C02")
// DEFINE_SBC(deco222_device, DECO222, deco222, "Data East DECO222")
// DEFINE_SBC(m740_device, M740, m740, "Mitsubishi M740")

// ----------------------------------------------------------------------------
// Additional Hitachi 6301/6303 variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(hd6301v1_cpu_device, HD6301V1, hd6301v1, "Hitachi HD6301V1") // Duplicate - already at line 939
// DEFINE_SBC(hd6301x_cpu_device, HD6301X, hd6301x, "Hitachi HD6301X")
// DEFINE_SBC(hd6301y_cpu_device, HD6301Y, hd6301y, "Hitachi HD6301Y")
DEFINE_SBC(hd6303x_cpu_device, HD6303X, hd6303x, "Hitachi HD6303X")

// ----------------------------------------------------------------------------
// Additional MCS-48 variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(upi41_cpu_device, UPI41, upi41, "Intel UPI-41")
// DEFINE_SBC(upi42_cpu_device, UPI42, upi42, "Intel UPI-42")
// DEFINE_SBC(i8243_device, I8243, i8243, "Intel 8243 I/O Expander")

// ----------------------------------------------------------------------------
// Additional Zilog Z80 variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(kc82_device, KC82, kc82, "East German KC82")

// ----------------------------------------------------------------------------
// Additional Intel x86 variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(i80188_device, I80188, i80188b, "Intel 80188 (alt)")
// DEFINE_SBC(am186es_device, AM186ES, am186es, "AMD Am186ES")
// DEFINE_SBC(am188es_device, AM188ES, am188es, "AMD Am188ES")

// ----------------------------------------------------------------------------
// Additional NEC V-series variants
// ----------------------------------------------------------------------------

DEFINE_SBC(v25_device, V25, v25, "NEC V25")
DEFINE_SBC(v35_device, V35, v35, "NEC V35")

// ----------------------------------------------------------------------------
// Additional 68000 variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(m68301_device, M68301, m68301, "Motorola 68301")
// DEFINE_SBC(mcf5204_device, MCF5204, mcf5204, "Freescale MCF5204 (ColdFire)")

// ----------------------------------------------------------------------------
// Additional SPARC variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(sparc7_device, SPARC7, sparc7, "Sun SPARC v7")

// ----------------------------------------------------------------------------
// Additional ARM variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(arm7_cpu_device, ARM7_CPU, arm7cpu, "ARM7 CPU")

// ----------------------------------------------------------------------------
// Additional PowerPC variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(ppc4xx_device, PPC4XX, ppc4xx, "PowerPC 4xx")

// ----------------------------------------------------------------------------
// Additional MIPS variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(r3041be_device, R3041BE, r3041be, "MIPS R3041 Big-Endian")

// ----------------------------------------------------------------------------
// Additional DSP variants
// ----------------------------------------------------------------------------

// DEFINE_SBC(dsp56k_device, DSP56K, dsp56k, "Motorola DSP56000")

// ----------------------------------------------------------------------------
// Additional microcontroller variants
// ----------------------------------------------------------------------------

// More 8051 variants
DEFINE_SBC(i8751_device, I8751, i8751, "Intel 8751")
DEFINE_SBC(i8752_device, I8752, i8752, "Intel 8752")
DEFINE_SBC(i80c51gb_device, I80C51GB, i80c51gb, "Intel 80C51GB")
// DEFINE_SBC(at89c52_device, AT89C52, at89c52, "Atmel AT89C52") // BROKEN: validation fails
// DEFINE_SBC(at89s52_device, AT89S52, at89s52, "Atmel AT89S52") // BROKEN: validation fails

// More PIC variants
// DEFINE_SBC(pic16c5x_device, PIC16C5X, pic16c5x, "Microchip PIC16C5x")
// DEFINE_SBC(pic16c84_device, PIC16C84, pic16c84, "Microchip PIC16C84")

// More H8 variants
// DEFINE_SBC(h8330_device, H8330, h8330, "Hitachi H8/330")
DEFINE_SBC(h83002_device, H83002, h83002, "Hitachi H8/3002")
DEFINE_SBC(h83003_device, H83003, h83003, "Hitachi H8/3003")
// DEFINE_SBC(h83004_device, H83004, h83004, "Hitachi H8/3004")
DEFINE_SBC(h83006_device, H83006, h83006, "Hitachi H8/3006")
DEFINE_SBC(h83007_device, H83007, h83007, "Hitachi H8/3007")
DEFINE_SBC(h83008_device, H83008, h83008, "Hitachi H8/3008")
DEFINE_SBC(h83044_device, H83044, h83044, "Hitachi H8/3044")
DEFINE_SBC(h83045_device, H83045, h83045, "Hitachi H8/3045")

// More 68HC05 variants
// DEFINE_SBC(m68hc705c8_device, M68HC705C8, m68hc705c8, "Motorola 68HC705C8")

// More 68HC11 variants
DEFINE_SBC(mc68hc11a1_device, MC68HC11A1, mc68hc11a1, "Motorola MC68HC11A1")

// More uPD78xx variants
// DEFINE_SBC(upd78c058_device, UPD78C058, upd78c058, "NEC uPD78C058")

// More AVR variants
// DEFINE_SBC(atmega8_device, ATMEGA8, atmega8, "Atmel ATmega8")
// DEFINE_SBC(atmega16_device, ATMEGA16, atmega16, "Atmel ATmega16")
// DEFINE_SBC(atmega32_device, ATMEGA32, atmega32, "Atmel ATmega32")
// DEFINE_SBC(atmega48_device, ATMEGA48, atmega48, "Atmel ATmega48")
// DEFINE_SBC(atmega128_device, ATMEGA128, atmega128, "Atmel ATmega128")
// DEFINE_SBC(atmega168_device, ATMEGA168, atmega168, "Atmel ATmega168") // BROKEN: validation fails
// DEFINE_SBC(atmega328_device, ATMEGA328, atmega328, "Atmel ATmega328") // BROKEN: validation fails
// DEFINE_SBC(attiny15_device, ATTINY15, attiny15, "Atmel ATtiny15") // BROKEN: validation fails
// DEFINE_SBC(attiny25_device, ATTINY25, attiny25, "Atmel ATtiny25")
// DEFINE_SBC(attiny45_device, ATTINY45, attiny45, "Atmel ATtiny45")
// DEFINE_SBC(attiny85_device, ATTINY85, attiny85, "Atmel ATtiny85")

// ----------------------------------------------------------------------------
// Additional 4-bit microcontrollers
// ----------------------------------------------------------------------------

// More TMS1000 variants
// DEFINE_SBC(tms1000c_device, TMS1000C, tms1000c, "TI TMS1000C")
// DEFINE_SBC(tms1040_device, TMS1040, tms1040, "TI TMS1040")
// DEFINE_SBC(tms1170_device, TMS1170, tms1170, "TI TMS1170")
// DEFINE_SBC(tms1270_device, TMS1270, tms1270, "TI TMS1270")
// DEFINE_SBC(tms1370_device, TMS1370, tms1370, "TI TMS1370")
// DEFINE_SBC(tms1400_device, TMS1400, tms1400, "TI TMS1400")
// DEFINE_SBC(tms1470_device, TMS1470, tms1470, "TI TMS1470")
// DEFINE_SBC(tms1600_device, TMS1600, tms1600, "TI TMS1600")
// DEFINE_SBC(tms1670_device, TMS1670, tms1670, "TI TMS1670")
// DEFINE_SBC(tms1700_device, TMS1700, tms1700, "TI TMS1700")
// DEFINE_SBC(tms0970_device, TMS0970, tms0970, "TI TMS0970")
// DEFINE_SBC(tms0950_device, TMS0950, tms0950, "TI TMS0950")
// DEFINE_SBC(tmc0271_device, TMC0271, tmc0271, "TI TMC0271")
// DEFINE_SBC(tmc0281_device, TMC0281, tmc0281, "TI TMC0281")
// DEFINE_SBC(tms0172_device, TMS0172, tms0172, "TI TMS0172")
// DEFINE_SBC(tms1980_device, TMS1980, tms1980, "TI TMS1980")
// DEFINE_SBC(tp0320_device, TP0320, tp0320, "TI TP0320")

// More COP400 variants
// DEFINE_SBC(cop401_device, COP401, cop401, "National COP401")
// DEFINE_SBC(cop402_device, COP402, cop402, "National COP402")
// DEFINE_SBC(cop404_device, COP404, cop404, "National COP404")
// DEFINE_SBC(cop424_device, COP424, cop424, "National COP424")
// DEFINE_SBC(cop425_device, COP425, cop425, "National COP425")
// DEFINE_SBC(cop426_device, COP426, cop426, "National COP426")

// More HMCS40 variants
// DEFINE_SBC(hmcs400_device, HMCS400, hmcs400, "Hitachi HMCS400")

// More Sharp SM variants
// DEFINE_SBC(sm5k_device, SM5K, sm5k, "Sharp SM5K")
// DEFINE_SBC(kb1013vk12_device, KB1013VK12, kb1013vk12, "KB1013VK12 (SM510 clone)")

// More NEC uCOM-4 variants
// DEFINE_SBC(upd552_device, UPD552, upd552, "NEC uPD552 (uCOM-4)")
// DEFINE_SBC(upd555_device, UPD555, upd555, "NEC uPD555 (uCOM-4)")
// DEFINE_SBC(upd556_device, UPD556, upd556, "NEC uPD556 (uCOM-4)")
// DEFINE_SBC(upd557l_device, UPD557L, upd557l, "NEC uPD557L (uCOM-4)")
// DEFINE_SBC(upd558_device, UPD558, upd558, "NEC uPD558 (uCOM-4)")

// ----------------------------------------------------------------------------
// Additional game/graphics CPUs
// ----------------------------------------------------------------------------

// More PlayStation variants
DEFINE_SBC(cxd8606bq_device, CXD8606BQ, cxd8606bq, "Sony CXD8606BQ (PS1)")
DEFINE_SBC(cxd8606cq_device, CXD8606CQ, cxd8606cq, "Sony CXD8606CQ (PS1)")

// More Konami variants
// DEFINE_SBC(konami1_device, KONAMI1, konami1, "Konami-1 (Time Pilot)")

// ----------------------------------------------------------------------------
// Additional vintage/historic CPUs
// ----------------------------------------------------------------------------

// More DEC CPUs
DEFINE_SBC(lsi11_device, LSI11, lsi11, "DEC LSI-11")
// DEFINE_SBC(pdp11_device, PDP11, pdp11, "DEC PDP-11")

// ----------------------------------------------------------------------------
// Additional calculator CPUs
// ----------------------------------------------------------------------------

// More Sharp calculator CPUs
// DEFINE_SBC(lh5801_device, LH5801, lh5801, "Sharp LH5801")

// More HP calculator CPUs
// DEFINE_SBC(nanoprocessor_device, NANOPROCESSOR, nanoprocessor, "HP Nanoprocessor")

// ----------------------------------------------------------------------------
// Additional misc CPUs
// ----------------------------------------------------------------------------

// More Signetics variants
// DEFINE_SBC(s2636_device, S2636, s2636, "Signetics 2636 PVI")

// More National variants
// DEFINE_SBC(pace_device, PACE, pace, "National PACE")

// More Hitachi variants
// DEFINE_SBC(hd404_device, HD404, hd404, "Hitachi HD404")
// DEFINE_SBC(hd44780_device, HD44780, hd44780, "Hitachi HD44780 LCD Controller")
// DEFINE_SBC(hd63701_cpu_device, HD63701, hd63701, "Hitachi HD63701")
DEFINE_SBC(hd6309e_device, HD6309E, hd6309e, "Hitachi HD6309E")

// More Motorola variants
DEFINE_SBC(mc68120_device, MC68120, mc68120, "Motorola 68120")

// More Toshiba variants
DEFINE_SBC(tmp95c061_device, TMP95C061, tmp95c061, "Toshiba TMP95C061")
DEFINE_SBC(tmp95c063_device, TMP95C063, tmp95c063, "Toshiba TMP95C063")

// More Epson variants
// DEFINE_SBC(s1c88_device, S1C88, s1c88, "Epson S1C88")

// More Sanyo variants
// DEFINE_SBC(lc58_device, LC58, lc58, "Sanyo LC58")
// DEFINE_SBC(lc8109_device, LC8109, lc8109, "Sanyo LC8109")

// More Fujitsu variants
// DEFINE_SBC(mb88_device, MB88, mb88, "Fujitsu MB88xx")
// DEFINE_SBC(mb8841_device, MB8841, mb8841, "Fujitsu MB8841")

// More Sony variants
// DEFINE_SBC(mn10200_device, MN10200, mn10200, "Matsushita MN10200")

// More misc 8-bit CPUs
// DEFINE_SBC(e0c6200_device, E0C6200, e0c6200, "Seiko Epson E0C6200")
// DEFINE_SBC(hpc_device, HPC, hpc, "HP HPC Hybrid Processor")
// DEFINE_SBC(sm8500_device, SM8500, sm8500, "Sharp SM8500")
// DEFINE_SBC(melps4_device, MELPS4, melps4, "Mitsubishi MELPS 4")
// DEFINE_SBC(melps740_device, MELPS740, melps740, "Mitsubishi MELPS 740")

// More misc 16-bit CPUs
// DEFINE_SBC(ccpu_device, CCPU, ccpu, "Cinematronics CCPU")
// DEFINE_SBC(cp1610_device, CP1610, cp1610, "GI CP1610")
// DEFINE_SBC(pdp8_device, PDP8_CPU, pdp8cpu, "DEC PDP-8 CPU")

// More misc 32-bit CPUs
// DEFINE_SBC(hyperstone_e132n_device, E132N, e132n, "Hyperstone E1-32N")
// DEFINE_SBC(hyperstone_e132t_device, E132T, e132t, "Hyperstone E1-32T")
// DEFINE_SBC(hyperstone_e132xt_device, E132XT, e132xt, "Hyperstone E1-32XT")
// DEFINE_SBC(hyperstone_e132xs_device, E132XS, e132xs, "Hyperstone E1-32XS")
// DEFINE_SBC(hyperstone_e116t_device, E116T, e116t, "Hyperstone E1-16T")
// DEFINE_SBC(hyperstone_e116xt_device, E116XT, e116xt, "Hyperstone E1-16XT")
// DEFINE_SBC(hyperstone_e116xs_device, E116XS, e116xs, "Hyperstone E1-16XS")
// DEFINE_SBC(hyperstone_gms30c2116_device, GMS30C2116, gms30c2116, "Hyperstone GMS30C2116")
// DEFINE_SBC(hyperstone_gms30c2132_device, GMS30C2132, gms30c2132, "Hyperstone GMS30C2132")
// DEFINE_SBC(hyperstone_gms30c2216_device, GMS30C2216, gms30c2216, "Hyperstone GMS30C2216")
// DEFINE_SBC(hyperstone_gms30c2232_device, GMS30C2232, gms30c2232, "Hyperstone GMS30C2232")

// Patinhofeio
// DEFINE_SBC(patinho_feio_cpu_device, PATINHOFEIO, patinhofeio, "Patinho Feio")

// Cosmac Elf
DEFINE_SBC(cdp1801_device, CDP1801, cdp1801, "RCA CDP1801 (COSMAC)")
DEFINE_SBC(cdp1804_device, CDP1804, cdp1804, "RCA CDP1804 (COSMAC)")
DEFINE_SBC(cdp1806_device, CDP1806, cdp1806, "RCA CDP1806 (COSMAC)")

// RCA COSMAC more variants
// DEFINE_SBC(cdp1869_device, CDP1869, cdp1869, "RCA CDP1869 Video")

// Misc gaming CPUs
DEFINE_SBC(unsp_device, UNSP, unsp, "SunPlus unSP")
// DEFINE_SBC(unsp11_device, UNSP11, unsp11, "SunPlus unSP v1.1")
// DEFINE_SBC(unsp12_device, UNSP12, unsp12, "SunPlus unSP v1.2")
// DEFINE_SBC(unsp20_device, UNSP20, unsp20, "SunPlus unSP v2.0")

// More SMC
// DEFINE_SBC(sm8521_device, SM8521, sm8521, "Sharp SM8521")

// More Toshiba TLCS variants
// DEFINE_SBC(tlcs90_device, TLCS90, tlcs90, "Toshiba TLCS-90")
// DEFINE_SBC(tlcs900h_device, TLCS900H, tlcs900h, "Toshiba TLCS-900/H")

// More Zilog Z8 variants
DEFINE_SBC(z8601_device, Z8601, z8601, "Zilog Z8601")
DEFINE_SBC(z8611_device, Z8611, z8611, "Zilog Z8611")

// More Epson variants
// DEFINE_SBC(s1c17_device, S1C17, s1c17, "Seiko Epson S1C17")
// DEFINE_SBC(e0c88_device, E0C88, e0c88, "Seiko Epson E0C88")

// More NEC variants
// DEFINE_SBC(upd70008_device, UPD70008, upd70008, "NEC uPD70008 (V20 variant)")
// DEFINE_SBC(upd70116_device, UPD70116, upd70116, "NEC uPD70116 (V30 variant)")

// More Motorola variants
// DEFINE_SBC(m146805_device, M146805, m146805, "Motorola MC146805")

// Texas Instruments TMS34010 (Graphics CPU)
DEFINE_SBC(tms34010_device, TMS34010, tms34010, "TI TMS34010 (Graphics)")
DEFINE_SBC(tms34020_device, TMS34020, tms34020, "TI TMS34020 (Graphics)")

// MOS 65xx I/O CPUs
// DEFINE_SBC(m6504_device, M6504_ALT, m6504alt, "MOS 6504 (alt)")
// DEFINE_SBC(m6505_device, M6505, m6505, "MOS 6505")
// DEFINE_SBC(m6506_device, M6506, m6506, "MOS 6506")
DEFINE_SBC(m6512_device, M6512, m6512, "MOS 6512")
// DEFINE_SBC(m6513_device, M6513, m6513, "MOS 6513")
// DEFINE_SBC(m6514_device, M6514, m6514, "MOS 6514")
// DEFINE_SBC(m6515_device, M6515, m6515, "MOS 6515")

// RCA CDP1800 variants
// DEFINE_SBC(cdp1800_device, CDP1800, cdp1800, "RCA CDP1800")

// GI-1600 variants
// DEFINE_SBC(cp1600_device, CP1600_ALT, cp1600alt, "GI CP1600 (alt)")

// PACE family
// DEFINE_SBC(imp_device, IMP, imp, "National IMP")

// More I8x9x variants
// DEFINE_SBC(i8x9x_device, I8X9X, i8x9x, "Intel I8x9x family")

// Western Electric WE DSP3x
// DEFINE_SBC(dsp3x_device, DSP3X, dsp3x, "WE DSP3x")

// EA Risc
// DEFINE_SBC(ea_risc_device, EARISC, earisc, "EA RISC")

// Trimedia TM1000
// DEFINE_SBC(tm1000_device, TM1000, tm1000, "Philips TriMedia TM1000")

// Videologic Neon250 PVR
// DEFINE_SBC(neon_device, NEON, neon, "VideoLogic Neon250")

// Sega Model 2/3 GEO
DEFINE_SBC(mb86234_device, MB86234, mb86234, "Fujitsu MB86234 (GEO)")
DEFINE_SBC(mb86235_device, MB86235, mb86235, "Fujitsu MB86235 (GEO)")

// NEC V810
DEFINE_SBC(v810_device, V810, v810, "NEC V810")

// NEC V830
// DEFINE_SBC(v830_device, V830, v830, "NEC V830")

// NEC V850
// DEFINE_SBC(v850_device, V850, v850, "NEC V850")
// DEFINE_SBC(v850e_device, V850E, v850e, "NEC V850E")
// DEFINE_SBC(v850es_device, V850ES, v850es, "NEC V850ES")

// ARC cores
// DEFINE_SBC(arc_device, ARC, arc, "ARC Tangent-A5")

// Axis ETRAX
// DEFINE_SBC(etrax_device, ETRAX, etrax, "Axis ETRAX")

// Altera Nios
// DEFINE_SBC(nios_device, NIOS, nios, "Altera Nios")

// Altera Nios2
DEFINE_SBC(nios2_device, NIOS2, nios2, "Altera Nios II")

// Xilinx MicroBlaze
// DEFINE_SBC(microblaze_device, MICROBLAZE, microblaze, "Xilinx MicroBlaze")

// OpenRISC OR1200
// DEFINE_SBC(or1200_device, OR1200, or1200, "OpenRISC OR1200")

// Lattice LM32
// DEFINE_SBC(lm32_device, LM32, lm32, "Lattice Mico32")

// Renesas M32R
// DEFINE_SBC(m32r_device, M32R, m32r, "Renesas M32R")

// Renesas M32C
// DEFINE_SBC(m16c_device, M16C, m16c, "Renesas M16C")
// DEFINE_SBC(m32c_device, M32C, m32c, "Renesas M32C")

// Renesas RX
// DEFINE_SBC(rx_device, RX, rx, "Renesas RX")

// Renesas SH2E
// DEFINE_SBC(sh2e_device, SH2E, sh2e, "Hitachi SH-2E")

// More AVR32
// DEFINE_SBC(avr32_device, AVR32, avr32, "Atmel AVR32")

// More ARM Cortex
// DEFINE_SBC(cortex_m0_device, CORTEX_M0, cortexm0, "ARM Cortex-M0")
// DEFINE_SBC(cortex_m3_device, CORTEX_M3, cortexm3, "ARM Cortex-M3")
// DEFINE_SBC(cortex_m4_device, CORTEX_M4, cortexm4, "ARM Cortex-M4")

// Sony SPU
// DEFINE_SBC(spu_device, SPU, spu, "Sony SPU (PS1 Audio)")

// Capcom DL-1425
// DEFINE_SBC(dl1425_device, DL1425, dl1425, "Capcom DL-1425")

// E8870
// DEFINE_SBC(e8870_device, E8870, e8870, "Intel E8870")

// EISC
// DEFINE_SBC(eisc_device, EISC, eisc, "eSi-RISC (EISC)")

// MN1400 family (more Panasonic CPUs)
// DEFINE_SBC(mn1400_device, MN1400, mn1400, "Panasonic MN1400")
// DEFINE_SBC(mn1500_device, MN1500, mn1500, "Panasonic MN1500")
// DEFINE_SBC(mn1610_device, MN1610, mn1610, "Panasonic MN1610")
// DEFINE_SBC(mn1613_device, MN1613, mn1613, "Panasonic MN1613")
// DEFINE_SBC(mn1870_device, MN1870, mn1870, "Panasonic MN1870")
DEFINE_SBC(mn1880_device, MN1880, mn1880, "Panasonic MN1880")

// More Mitsubishi M377xx
// DEFINE_SBC(m37702_device, M37702, m37702, "Mitsubishi M37702")
// DEFINE_SBC(m37703_device, M37703, m37703, "Mitsubishi M37703")
// DEFINE_SBC(m37720_device, M37720, m37720, "Mitsubishi M37720")

// Mitsubishi M58800
// DEFINE_SBC(m58800_device, M58800, m58800, "Mitsubishi M58800")

// Mitsubishi M58840
// DEFINE_SBC(m58840_device, M58840, m58840, "Mitsubishi M58840")

// More Toshiba TX variants
// DEFINE_SBC(tx79_device, TX79, tx79, "Toshiba TX79 (PS2 EE variant)")

// PDP-10
// DEFINE_SBC(pdp10_device, PDP10, pdp10, "DEC PDP-10")

// PDP-4
// DEFINE_SBC(pdp4_device, PDP4, pdp4, "DEC PDP-4")

// KIM-1 variant
// DEFINE_SBC(m6530_device, M6530, m6530, "MOS 6530 (RRIOT)")

// Ohio Scientific variant
// DEFINE_SBC(osi_device, OSI, osi, "Ohio Scientific 6502")

// More ST Micro variants
// DEFINE_SBC(st6_device, ST6, st6, "ST Microelectronics ST6")
// DEFINE_SBC(st7_device, ST7, st7, "ST Microelectronics ST7")
// DEFINE_SBC(stm8_device, STM8, stm8, "ST Microelectronics STM8")

// More Texas Instruments MSP430
// DEFINE_SBC(msp430_device, MSP430, msp430, "TI MSP430")

// More EM Microelectronic
// DEFINE_SBC(em4325_device, EM4325, em4325, "EM Microelectronic EM4325")

// More Freescale/NXP
// DEFINE_SBC(hcs08_device, HCS08, hcs08, "Freescale HCS08")
// DEFINE_SBC(s08_device, S08, s08, "Freescale S08")

// More Freescale RS08
// DEFINE_SBC(rs08_device, RS08, rs08, "Freescale RS08")

// More Siemens/Infineon
// DEFINE_SBC(c166_device, C166, c166, "Siemens C166")
// DEFINE_SBC(xc800_device, XC800, xc800, "Infineon XC800")

// More Rabbit Semiconductor
// DEFINE_SBC(rabbit2000_device, RABBIT2000, rabbit2000, "Rabbit Semiconductor Rabbit 2000")
// DEFINE_SBC(rabbit3000_device, RABBIT3000, rabbit3000, "Rabbit Semiconductor Rabbit 3000")

// More Parallax
// DEFINE_SBC(propeller_device, PROPELLER, propeller, "Parallax Propeller")

// More XMOS
// DEFINE_SBC(xcore_device, XCORE, xcore, "XMOS XCore")

// More PIC18
// DEFINE_SBC(pic18_device, PIC18, pic18, "Microchip PIC18")

// More PIC24
// DEFINE_SBC(pic24_device, PIC24, pic24, "Microchip PIC24")

// More dsPIC
// DEFINE_SBC(dspic30f_device, DSPIC30F, dspic30f, "Microchip dsPIC30F")
// DEFINE_SBC(dspic33f_device, DSPIC33F, dspic33f, "Microchip dsPIC33F")

// More Digital Alpha variants
// DEFINE_SBC(alpha_ev4_device, ALPHA_EV4, alphaev4, "DEC Alpha 21064 EV4")
// DEFINE_SBC(alpha_ev5_device, ALPHA_EV5, alphaev5, "DEC Alpha 21164 EV5")
// DEFINE_SBC(alpha_ev6_device, ALPHA_EV6, alphaev6, "DEC Alpha 21264 EV6")

// HP PA-RISC
// DEFINE_SBC(hppa_device, HPPA, hppa, "HP PA-RISC")

// IBM POWER
// DEFINE_SBC(power_device, POWER, power, "IBM POWER")

// DEC Prism
// DEFINE_SBC(prism_device, PRISM, prism, "DEC PRISM")

// Emotion Engine components
// DEFINE_SBC(ee_device, EE, ee, "Sony Emotion Engine (PS2)")
// DEFINE_SBC(vu0_device, VU0, vu0, "Sony VU0 (PS2)")
// DEFINE_SBC(vu1_device, VU1, vu1, "Sony VU1 (PS2)")

// Graphics Synthesizer CPU
// DEFINE_SBC(gs_device, GS, gs, "Sony Graphics Synthesizer (PS2)")

// More I8008 variants
// DEFINE_SBC(i8008_1_device, I8008_1, i80081, "Intel 8008-1")

// More NatSemi variants
// DEFINE_SBC(gmc4_device, GMC4, gmc4, "National GMC-4")
