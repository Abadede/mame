// license:BSD-3-Clause
// copyright-holders:Alex Pasadyn, Zsolt Vasvari, Ernesto Corvi, Aaron Giles
// thanks-to:Kurt Mahan
/*************************************************************************

    Williams/Midway Y/Z-unit system

**************************************************************************/

#include "emu.h"
#include "cpu/m6809/m6809.h"
#include "midyunit.h"


/* constant definitions */
#define SOUND_NARC                  1
#define SOUND_CVSD_SMALL            2
#define SOUND_CVSD                  3
#define SOUND_ADPCM                 4
#define SOUND_YAWDIM                5
#define SOUND_YAWDIM2               6



/*************************************
 *
 *  CMOS reads/writes
 *
 *************************************/

void midyunit_state::midyunit_cmos_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	logerror("%08x:CMOS Write @ %05X\n", m_maincpu->pc(), offset);
	COMBINE_DATA(&m_cmos_ram[offset + m_cmos_page]);
}


uint16_t midyunit_state::midyunit_cmos_r(offs_t offset)
{
	return m_cmos_ram[offset + m_cmos_page];
}



/*************************************
 *
 *  CMOS enable and protection
 *
 *************************************/

void midyunit_state::midyunit_cmos_enable_w(address_space &space, uint16_t data)
{
	m_cmos_w_enable = (~data >> 9) & 1;

	logerror("%08x:Protection write = %04X\n", m_maincpu->pc(), data);

	/* only go down this path if we have a data structure */
	if (m_prot_data)
	{
		/* mask off the data */
		data &= 0x0f00;

		/* update the FIFO */
		m_prot_sequence[0] = m_prot_sequence[1];
		m_prot_sequence[1] = m_prot_sequence[2];
		m_prot_sequence[2] = data;

		/* special case: sequence entry 1234 means Strike Force, which is different */
		if (m_prot_data->reset_sequence[0] == 0x1234)
		{
			if (data == 0x500)
			{
				m_prot_result = space.read_word(0x10a4390) << 4;
				logerror("  desired result = %04X\n", m_prot_result);
			}
		}

		/* all other games use the same pattern */
		else
		{
			/* look for a reset */
			if (m_prot_sequence[0] == m_prot_data->reset_sequence[0] &&
				m_prot_sequence[1] == m_prot_data->reset_sequence[1] &&
				m_prot_sequence[2] == m_prot_data->reset_sequence[2])
			{
				logerror("Protection reset\n");
				m_prot_index = 0;
			}

			/* look for a clock */
			if ((m_prot_sequence[1] & 0x0800) != 0 && (m_prot_sequence[2] & 0x0800) == 0)
			{
				m_prot_result = m_prot_data->data_sequence[m_prot_index++];
				logerror("Protection clock (new data = %04X)\n", m_prot_result);
			}
		}
	}
}


uint16_t midyunit_state::midyunit_protection_r()
{
	/* return the most recently clocked value */
	logerror("%08X:Protection read = %04X\n", m_maincpu->pc(), m_prot_result);
	return m_prot_result;
}



/*************************************
 *
 *  Generic input ports
 *
 *************************************/

uint16_t midyunit_state::midyunit_input_r(offs_t offset)
{
	return m_ports[offset]->read();
}



/*************************************
 *
 *  Special Terminator 2 input ports
 *
 *************************************/

uint16_t midyunit_state::term2_input_r(offs_t offset)
{
	if (offset != 2)
		return m_ports[offset]->read();

	return m_term2_adc->read() | 0xff00;
}

void midyunit_state::term2_sound_w(offs_t offset, uint16_t data)
{
	/* Flash Lamp Output Data */
	if  ( ((data & 0x800) != 0x800) && ((data & 0x400) == 0x400 ) )
	{
		output().set_value("Left_Flash_1", data & 0x1);
		output().set_value("Left_Flash_2", (data & 0x2) >> 1);
		output().set_value("Left_Flash_3", (data & 0x4) >> 2);
		output().set_value("Left_Flash_4", (data & 0x8) >> 3);
		output().set_value("Right_Flash_1", (data & 0x10) >> 4);
		output().set_value("Right_Flash_2", (data & 0x20) >> 5);
		output().set_value("Right_Flash_3", (data & 0x40) >> 6);
		output().set_value("Right_Flash_4", (data & 0x80) >> 7);
	}

	/* Gun Output Data */
	if  ( ((data & 0x800) == 0x800) && ((data & 0x400) != 0x400 ) )
	{
		output().set_value("Left_Gun_Recoil", data & 0x1);
		output().set_value("Right_Gun_Recoil", (data & 0x2) >> 1);
		output().set_value("Left_Gun_Green_Led", (~data & 0x20) >> 5);
		output().set_value("Left_Gun_Red_Led", (~data & 0x10) >> 4);
		output().set_value("Right_Gun_Green_Led", (~data & 0x80) >> 7);
		output().set_value("Right_Gun_Red_Led", (~data & 0x40) >> 6);
	}

	if (offset == 0)
		m_term2_adc->write(((data >> 12) & 3) | 4);

	m_adpcm_sound->reset_write((~data & 0x100) >> 1);
	m_adpcm_sound->write(data);
}



/*************************************
 *
 *  Special Terminator 2 hack
 *
 *************************************/

void midyunit_state::term2_hack_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (offset == 1 && m_maincpu->pc() == 0xffce6520)
	{
		m_t2_hack_mem[offset] = 0;
		return;
	}
	COMBINE_DATA(&m_t2_hack_mem[offset]);
}

void midyunit_state::term2la3_hack_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (offset == 0 && m_maincpu->pc() == 0xffce5230)
	{
		m_t2_hack_mem[offset] = 0;
		return;
	}
	COMBINE_DATA(&m_t2_hack_mem[offset]);
}

void midyunit_state::term2la2_hack_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (offset == 0 && m_maincpu->pc() == 0xffce4b80)
	{
		m_t2_hack_mem[offset] = 0;
		return;
	}
	COMBINE_DATA(&m_t2_hack_mem[offset]);
}

void midyunit_state::term2la1_hack_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (offset == 0 && m_maincpu->pc() == 0xffce33f0)
	{
		m_t2_hack_mem[offset] = 0;
		return;
	}
	COMBINE_DATA(&m_t2_hack_mem[offset]);
}



/*************************************
 *
 *  Generic driver init
 *
 *************************************/

void midyunit_state::cvsd_protection_w(offs_t offset, uint8_t data)
{
	/* because the entire CVSD ROM is banked, we have to make sure that writes */
	/* go to the proper location (i.e., bank 0); currently bank 0 always lives */
	/* in the 0x10000-0x17fff space, so we just need to add 0x8000 to get the  */
	/* proper offset */
	m_cvsd_protection_base[offset] = data;
}

void midyunit_state::install_hidden_ram(mc6809e_device &cpu, int prot_start, int prot_end)
{
	size_t size = 1 + prot_end - prot_start;
	m_hidden_ram = std::make_unique<uint8_t[]>(size);
	save_pointer(NAME(m_hidden_ram), size);
	cpu.space(AS_PROGRAM).install_ram(prot_start, prot_end, m_hidden_ram.get());
}

void midyunit_state::init_generic(int bpp, int sound, int prot_start, int prot_end)
{
	offs_t gfx_chunk = m_gfx_rom.bytes() / 4;
	uint8_t d1, d2, d3, d4, d5, d6;
	int i;

	/* load graphics ROMs */
	uint8_t *base = memregion("gfx1")->base();
	switch (bpp)
	{
		case 4:
			for (i = 0; i < m_gfx_rom.bytes(); i += 2)
			{
				d1 = ((base[0 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d2 = ((base[1 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d3 = ((base[0 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;
				d4 = ((base[1 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;

				m_gfx_rom[i + 0] = d1 | (d2 << 2);
				m_gfx_rom[i + 1] = d3 | (d4 << 2);
			}
			break;

		case 6:
			for (i = 0; i < m_gfx_rom.bytes(); i += 2)
			{
				d1 = ((base[0 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d2 = ((base[1 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d3 = ((base[2 * gfx_chunk + (i + 0) / 4]) >> (2 * ((i + 0) % 4))) & 3;
				d4 = ((base[0 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;
				d5 = ((base[1 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;
				d6 = ((base[2 * gfx_chunk + (i + 1) / 4]) >> (2 * ((i + 1) % 4))) & 3;

				m_gfx_rom[i + 0] = d1 | (d2 << 2) | (d3 << 4);
				m_gfx_rom[i + 1] = d4 | (d5 << 2) | (d6 << 4);
			}
			break;

		case 8:
			for (i = 0; i < m_gfx_rom.bytes(); i += 4)
			{
				m_gfx_rom[i + 0] = base[0 * gfx_chunk + i / 4];
				m_gfx_rom[i + 1] = base[1 * gfx_chunk + i / 4];
				m_gfx_rom[i + 2] = base[2 * gfx_chunk + i / 4];
				m_gfx_rom[i + 3] = base[3 * gfx_chunk + i / 4];
			}
			break;
	}

	/* load sound ROMs and set up sound handlers */
	m_chip_type = sound;
	switch (sound)
	{
		case SOUND_CVSD_SMALL:
			m_cvsd_sound->get_cpu()->space(AS_PROGRAM).install_write_handler(prot_start, prot_end, write8sm_delegate(*this, FUNC(midyunit_state::cvsd_protection_w)));
			m_cvsd_protection_base = memregion("cvsd:cpu")->base() + 0x10000 + (prot_start - 0x8000);
			break;

		case SOUND_CVSD:
			install_hidden_ram(*m_cvsd_sound->get_cpu(), prot_start, prot_end);
			break;

		case SOUND_ADPCM:
			install_hidden_ram(*m_adpcm_sound->get_cpu(), prot_start, prot_end);
			break;

		case SOUND_NARC:
			install_hidden_ram(*m_narc_sound->get_cpu(), prot_start, prot_end);
			break;

		case SOUND_YAWDIM:
		case SOUND_YAWDIM2:
			break;
	}
}



/*************************************
 *
 *  Z-unit init
 *
 *  music: 6809 driving YM2151, DAC
 *  effects: 6809 driving CVSD, DAC
 *
 *************************************/

void midyunit_state::init_narc()
{
	/* common init */
	init_generic(8, SOUND_NARC, 0xcdff, 0xce29);
}



/*************************************
 *
 *  Y-unit init (CVSD)
 *
 *  music: 6809 driving YM2151, DAC, and CVSD
 *
 *************************************/


/********************** Trog **************************/

void midyunit_state::init_trog()
{
	/* protection */
	static const struct protection_data trog_protection_data =
	{
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x3000, 0x1000, 0x2000, 0x0000,
			0x2000, 0x3000,
			0x3000, 0x1000,
			0x0000, 0x0000, 0x2000, 0x3000, 0x1000, 0x1000, 0x2000 }
	};
	m_prot_data = &trog_protection_data;

	/* common init */
	init_generic(4, SOUND_CVSD_SMALL, 0x9eaf, 0x9ed9);
}


/********************** Smash TV **********************/

void midyunit_state::init_smashtv()
{
	/* common init */
	init_generic(6, SOUND_CVSD_SMALL, 0x9cf6, 0x9d21);

	m_prot_data = nullptr;
}


/********************** High Impact Football **********************/

void midyunit_state::init_hiimpact()
{
	/* protection */
	static const struct protection_data hiimpact_protection_data =
	{
		{ 0x0b00, 0x0b00, 0x0b00 },
		{ 0x2000, 0x4000, 0x4000, 0x0000, 0x6000, 0x6000, 0x2000, 0x4000,
			0x2000, 0x4000, 0x2000, 0x0000, 0x4000, 0x6000, 0x2000 }
	};
	m_prot_data = &hiimpact_protection_data;

	/* common init */
	init_generic(6, SOUND_CVSD, 0x9b79, 0x9ba3);
}


/********************** Super High Impact Football **********************/

void midyunit_state::init_shimpact()
{
	/* protection */
	static const struct protection_data shimpact_protection_data =
	{
		{ 0x0f00, 0x0e00, 0x0d00 },
		{ 0x0000, 0x4000, 0x2000, 0x5000, 0x2000, 0x1000, 0x4000, 0x6000,
			0x3000, 0x0000, 0x2000, 0x5000, 0x5000, 0x5000, 0x2000 }
	};
	m_prot_data = &shimpact_protection_data;

	/* common init */
	init_generic(6, SOUND_CVSD, 0x9c06, 0x9c15);
}


/********************** Strike Force **********************/

void midyunit_state::init_strkforc()
{
	/* protection */
	static const struct protection_data strkforc_protection_data =
	{
		{ 0x1234 }
	};
	m_prot_data = &strkforc_protection_data;

	/* common init */
	init_generic(4, SOUND_CVSD_SMALL, 0x9f7d, 0x9fa7);
}



/*************************************
 *
 *  Y-unit init (ADPCM)
 *
 *  music: 6809 driving YM2151, DAC, and OKIM6295
 *
 *************************************/


/********************** Mortal Kombat **********************/

void midyunit_state::init_mkyunit()
{
	/* protection */
	static const struct protection_data mk_protection_data =
	{
		{ 0x0d00, 0x0c00, 0x0900 },
		{ 0x4600, 0xf600, 0xa600, 0x0600, 0x2600, 0x9600, 0xc600, 0xe600,
			0x8600, 0x7600, 0x8600, 0x8600, 0x9600, 0xd600, 0x6600, 0xb600,
			0xd600, 0xe600, 0xf600, 0x7600, 0xb600, 0xa600, 0x3600 }
	};
	m_prot_data = &mk_protection_data;

	/* common init */
	init_generic(6, SOUND_ADPCM, 0xfb9c, 0xfbc6);
}

void midyunit_state::init_mkyawdim()
{
	/* common init */
	init_generic(6, SOUND_YAWDIM, 0, 0);
}

void midyunit_state::init_mkyawdim2()
{
	m_audiocpu->space(AS_PROGRAM).install_write_handler(0x9000, 0x97ff, write8smo_delegate(*this, FUNC(midyunit_state::yawdim2_oki_bank_w)));
	m_audiocpu->space(AS_PROGRAM).install_read_handler(0xa000, 0xa7ff, read8smo_delegate(*this, FUNC(midyunit_state::yawdim2_soundlatch_r)));
	init_mkyawdim();
}


/*************************************
 *
 *  MK Turbo Ninja protection
 *
 *************************************/

uint16_t midyunit_state::mkturbo_prot_r()
{
	/* the security GAL overlays a counter of some sort at 0xfffff400 in ROM space.
	 * A startup protection check expects to read back two different values in succession */
	return machine().rand();
}

void midyunit_state::init_mkyturbo()
{
	/* protection */
	m_maincpu->space(AS_PROGRAM).install_read_handler(0xfffff400, 0xfffff40f, read16smo_delegate(*this, FUNC(midyunit_state::mkturbo_prot_r)));

	init_mkyunit();
}

void midyunit_state::init_mkla3bl()
{
	// rearrange GFX so the driver can deal with them
	uint8_t *gfxrom = memregion("gfx1")->base();
	std::vector<uint8_t> buffer(0x400000);
	memcpy(&buffer[0], gfxrom, 0x400000);

	for (int i = 0x000000; i < 0x100000; i++)
		gfxrom[i] = buffer[0x200000 | ((i & 0xfffff) * 2 + 1)];

	for (int i = 0x100000; i < 0x200000; i++)
		gfxrom[i] = buffer[(i & 0xfffff) * 2 + 1];

	for (int i = 0x200000; i < 0x300000; i++)
		gfxrom[i] = buffer[0x200000 | ((i & 0xfffff) * 2)];

	for (int i = 0x300000; i < 0x400000; i++)
		gfxrom[i] = buffer[(i & 0xfffff) * 2];

	// 0x400000 - 0x5fffff range is already ok

	init_mkyunit();
}

/********************** Terminator 2 **********************/

void midyunit_state::term2_init_common(write16s_delegate hack_w)
{
	// protection
	static constexpr struct protection_data term2_protection_data =
	{
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x4000, 0xf000, 0xa000 }
	};
	m_prot_data = &term2_protection_data;

	// common init
	init_generic(6, SOUND_ADPCM, 0xfa8d, 0xfa9c);

	// special inputs */
	m_maincpu->space(AS_PROGRAM).install_read_handler(0x01c00000, 0x01c0005f, read16sm_delegate(*this, FUNC(midyunit_state::term2_input_r)));
	m_maincpu->space(AS_PROGRAM).install_write_handler(0x01e00000, 0x01e0001f, write16sm_delegate(*this, FUNC(midyunit_state::term2_sound_w)));

	// HACK: this prevents the freeze on the movies
	// until we figure what's causing it, this is better than nothing
	m_maincpu->space(AS_PROGRAM).install_write_handler(0x010aa0e0, 0x010aa0ff, hack_w);
	m_t2_hack_mem = m_mainram + (0xaa0e0>>4);
}

void midyunit_state::init_term2()    { term2_init_common(write16s_delegate(*this, FUNC(midyunit_state::term2_hack_w))); }
void midyunit_state::init_term2la3() { term2_init_common(write16s_delegate(*this, FUNC(midyunit_state::term2la3_hack_w))); }
void midyunit_state::init_term2la2() { term2_init_common(write16s_delegate(*this, FUNC(midyunit_state::term2la2_hack_w))); }
void midyunit_state::init_term2la1() { term2_init_common(write16s_delegate(*this, FUNC(midyunit_state::term2la1_hack_w))); }



/********************** Total Carnage **********************/

void midyunit_state::init_totcarn()
{
	/* protection */
	static const struct protection_data totcarn_protection_data =
	{
		{ 0x0f00, 0x0f00, 0x0f00 },
		{ 0x4a00, 0x6a00, 0xda00, 0x6a00, 0x9a00, 0x4a00, 0x2a00, 0x9a00, 0x1a00,
			0x8a00, 0xaa00 }
	};
	m_prot_data = &totcarn_protection_data;

	/* common init */
	init_generic(6, SOUND_ADPCM, 0xfc04, 0xfc2e);
}



/*************************************
 *
 *  Machine init
 *
 *************************************/

MACHINE_RESET_MEMBER(midyunit_state,midyunit)
{
	/* reset sound */
	switch (m_chip_type)
	{
		case SOUND_NARC:
			m_narc_sound->reset_write(1);
			m_narc_sound->reset_write(0);
			break;

		case SOUND_CVSD:
		case SOUND_CVSD_SMALL:
			m_cvsd_sound->reset_write(1);
			m_cvsd_sound->reset_write(0);
			break;

		case SOUND_ADPCM:
			m_adpcm_sound->reset_write(1);
			m_adpcm_sound->reset_write(0);
			break;

		case SOUND_YAWDIM:
		case SOUND_YAWDIM2:
			break;
	}
}



/*************************************
 *
 *  Sound write handlers
 *
 *************************************/

void midyunit_state::midyunit_sound_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	/* check for out-of-bounds accesses */
	if (offset)
	{
		logerror("%08X:Unexpected write to sound (hi) = %04X\n", m_maincpu->pc(), data);
		return;
	}

	/* call through based on the sound type */
	if (ACCESSING_BITS_0_7 && ACCESSING_BITS_8_15)
		switch (m_chip_type)
		{
			case SOUND_NARC:
				m_narc_sound->write(data);
				break;

			case SOUND_CVSD_SMALL:
			case SOUND_CVSD:
				m_cvsd_sound->reset_write((~data & 0x100) >> 8);
				m_cvsd_sound->write((data & 0xff) | ((data & 0x200) >> 1));
				break;

			case SOUND_ADPCM:
				m_adpcm_sound->reset_write((~data & 0x100) >> 8);
				m_adpcm_sound->write(data);
				break;

			case SOUND_YAWDIM:
				m_soundlatch->write(data);
				m_audiocpu->pulse_input_line(INPUT_LINE_NMI, attotime::zero);
				break;

			case SOUND_YAWDIM2:
				m_soundlatch->write(data);
				m_audiocpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);
				break;
		}
}
