/***************************************************************************************
 *  Genesis Plus
 *  68k bus controller
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Eke-Eke (2007,2008,2009), additional code & fixes for the GCN/Wii port
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ****************************************************************************************/

#include "m68k.h"
#include "shared.h"
#include <imagine/logger/logger.h>
#ifndef NO_SCD
#include <scd/scd.h>
#endif

/*--------------------------------------------------------------------------*/
/* Unused area (return open bus data, i.e prefetched instruction word)      */
/*--------------------------------------------------------------------------*/
unsigned int m68k_read_bus_8(unsigned int address)
{
  logMsg("Unused read8 %08X (%08X)", address, m68k_get_reg (mm68k, M68K_REG_PC));
  return m68k_read_pcrelative_8(mm68k, mm68k.pc | (address & 1));
}

unsigned int m68k_read_bus_16(unsigned int address)
{
	logMsg("Unused read16 %08X (%08X)", address, m68k_get_reg (mm68k, M68K_REG_PC));
  return m68k_read_pcrelative_16(mm68k, mm68k.pc);
}


void m68k_unused_8_w (unsigned int address, unsigned int data)
{
	logMsg("Unused write8 %08X = %02X (%08X)", address, data, m68k_get_reg (mm68k, M68K_REG_PC));
}

void m68k_unused_16_w (unsigned int address, unsigned int data)
{
	logMsg("Unused write16 %08X = %04X (%08X)", address, data, m68k_get_reg (mm68k, M68K_REG_PC));
}


/*--------------------------------------------------------------------------*/
/* Illegal area (cause system to lock-up since !DTACK is not returned)      */
/*--------------------------------------------------------------------------*/
void m68k_lockup_w_8 (unsigned int address, unsigned int data)
{
	logMsg ("Lockup %08X = %02X (%08X)", address, data, m68k_get_reg (mm68k, M68K_REG_PC));
  if (!config_force_dtack)
  {
    m68k_pulse_halt(mm68k);
  }
}

void m68k_lockup_w_16 (unsigned int address, unsigned int data)
{
	logMsg ("Lockup %08X = %04X (%08X)", address, data, m68k_get_reg (mm68k, M68K_REG_PC));
  if (!config_force_dtack)
  {
    m68k_pulse_halt(mm68k);
  }
}

unsigned int m68k_lockup_r_8 (unsigned int address)
{ 
	logMsg ("Lockup %08X.b (%08X)", address, m68k_get_reg (mm68k, M68K_REG_PC));
  if (!config_force_dtack)
  {
    m68k_pulse_halt(mm68k);
  }
  return m68k_read_pcrelative_8(mm68k, mm68k.pc | (address & 1));
}

unsigned int m68k_lockup_r_16 (unsigned int address)
{
	logMsg ("Lockup %08X.w (%08X)", address, m68k_get_reg (mm68k, M68K_REG_PC));
  if (!config_force_dtack)
  {
    m68k_pulse_halt(mm68k);
  }
  return m68k_read_pcrelative_16(mm68k, mm68k.pc);
}


/*--------------------------------------------------------------------------*/
/* cartridge EEPROM                                                         */
/*--------------------------------------------------------------------------*/
unsigned int eeprom_read_byte(unsigned int address)
{
  if (address == eeprom.type.sda_out_adr)
  {
    return eeprom_read(0);
  }
  return READ_BYTE(cart.rom, address);
}

unsigned int eeprom_read_word(unsigned int address)
{
  if (address == (eeprom.type.sda_out_adr & 0xFFFFFE))
  {
    return eeprom_read(1);
  }
  return *(uint16a *)(cart.rom + address);
}

void eeprom_write_byte(unsigned int address, unsigned int data)
{
  if ((address == eeprom.type.sda_in_adr) || (address == eeprom.type.scl_adr))
  {
    eeprom_write(address, data, 0);
    return;
  }
  m68k_unused_8_w(address, data);
}

void eeprom_write_word(unsigned int address, unsigned int data)
{
  if ((address == (eeprom.type.sda_in_adr & 0xFFFFFE)) || (address == (eeprom.type.scl_adr & 0xFFFFFE)))
  {
    eeprom_write(address, data, 1);
    return;
  }
  m68k_unused_16_w (address, data);
}


/*--------------------------------------------------------------------------*/
/* Z80 bus (accessed through I/O chip)                                      */
/*--------------------------------------------------------------------------*/
unsigned int z80_read_byte(unsigned int address)
{
  switch ((address >> 13) & 3)
  {
    case 2:   /* YM2612 */
    {
      return fm_read(mm68k.cycleCount, address & 3);
    }

    case 3:   /* Misc  */
    {
      if ((address & 0xFF00) == 0x7F00)
      {
        /* VDP (through 68k bus) */
        return m68k_lockup_r_8(address);
      }
      return (m68k_read_bus_8(address) | 0xFF);
    }

    default: /* ZRAM */
    {
      return zram[address & 0x1FFF];
    }
  }
}

unsigned int z80_read_word(unsigned int address)
{
  unsigned int data = z80_read_byte(address);
  return (data | (data << 8));
}

void z80_write_byte(unsigned int address, unsigned int data)
{
  switch ((address >> 13) & 3)
  {
    case 2: /* YM2612 */
    {
      fm_write(mm68k.cycleCount, address & 3, data);
      return;
    }

    case 3:
    {
      switch ((address >> 8) & 0x7F)
      {
        case 0x60:  /* Bank register */
        {
          gen_zbank_w(data & 1);
          return;
        }

        case 0x7F:  /* VDP */
        {
          m68k_lockup_w_8(address, data);
          return;
        }
      
        default:
        {
          m68k_unused_8_w(address, data);
          return;
        }
      }
    }
      
    default: /* ZRAM */
    {
      zram[address & 0x1FFF] = data;
      mm68k.cycleCount += 8; /* ZRAM access latency (fixes Pacman 2: New Adventures) */
      return;
    }
  }
}

void z80_write_word(unsigned int address, unsigned int data)
{
  z80_write_byte(address, data >> 8);
}


/*--------------------------------------------------------------------------*/
/* I/O Control                                                              */
/*--------------------------------------------------------------------------*/
unsigned int ctrl_io_read_byte(unsigned int address)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        return io_68k_read((address >> 1) & 0x0F);
      }
      return m68k_read_bus_8(address);
    }

    case 0x11:  /* BUSACK */
    {
      if (!(address & 1))
      {
        unsigned int data = m68k_read_pcrelative_8(mm68k, mm68k.pc) & 0xFE;
        if (zstate == 3)
        {
          return data;
        }
        return (data | 0x01);
      }
      return m68k_read_bus_8(address);
    }

    case 0x30:  /* TIME */
    {
      if (cart.hw.time_r)
      {
        unsigned int data = cart.hw.time_r(address);
        if (address & 1)
        {
          return (data & 0xFF);
        }
        return (data >> 8);
      }
      return m68k_read_bus_8(address);
    }

    case 0x41:  /* OS ROM */
    {
      if (address & 1)
      {
        unsigned int data = m68k_read_pcrelative_8(mm68k, mm68k.pc) & 0xFE;
        return (gen_bankswitch_r() | data);
      }
      return m68k_read_bus_8(address);
    }

    case 0x10:  /* MEMORY MODE */
    case 0x12:  /* RESET */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x44:  /* RADICA */
    case 0x50:  /* SVP REGISTERS */
    {
      return m68k_read_bus_8(address);
    }

    default:  /* Invalid address */
    {
      return m68k_lockup_r_8(address);
    }
  }
}

unsigned int ctrl_io_read_word(unsigned int address)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        unsigned int data = io_68k_read((address >> 1) & 0x0F);
        return (data << 8 | data);
      }
      return m68k_read_bus_16(address); 
   }

    case 0x11:  /* BUSACK */
    {
      unsigned int data = m68k_read_pcrelative_16(mm68k, mm68k.pc) & 0xFEFF;
      if (zstate == 3)
      {
        return data;
      }
      return (data | 0x0100);
    }

    case 0x30:  /* TIME */
    {
      if (cart.hw.time_r)
      {
        return cart.hw.time_r(address);
      }
      return m68k_read_bus_16(address); 
    }
      
	#ifndef NO_SVP
    case 0x50:  /* SVP */
    {
      if ((address & 0xFD) == 0)
      {
        return svp->ssp1601.gr[SSP_XST].h;
      }

      if ((address & 0xFF) == 4)
      {
        unsigned int data = svp->ssp1601.gr[SSP_PM0].h;
        svp->ssp1601.gr[SSP_PM0].h &= ~1;
        return data;
      }

      return m68k_read_bus_16(address);
    }
	#endif

    case 0x10:  /* MEMORY MODE */
    case 0x12:  /* RESET */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x41:  /* OS ROM */
    case 0x44:  /* RADICA */
    {
      return m68k_read_bus_16(address);
    }

    default:  /* Invalid address */
    {
      return m68k_lockup_r_16(address);
    }
  }
}

void ctrl_io_write_byte(unsigned int address, unsigned int data)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if ((address & 0xE1) == 0x01)
      {
        /* get /LWR only */
        io_68k_write((address >> 1) & 0x0F, data);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x11:  /* BUSREQ */
    {
      if (!(address & 1))
      {
        gen_zbusreq_w(data & 1, mm68k.cycleCount);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x12:  /* RESET */
    {
      if (!(address & 1))
      {
        gen_zreset_w(data & 1, mm68k.cycleCount);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x30:  /* TIME */
    {
      cart.hw.time_w(address, data);
      return;
    }

    case 0x41:  /* OS ROM */
    {
      if (address & 1)
      {
        gen_bankswitch_w(data & 1);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x10:  /* MEMORY MODE */
    case 0x20:  /* MEGA-CD */
    case 0x40:  /* TMSS */
    case 0x44:  /* RADICA */
    case 0x50:  /* SVP REGISTERS */
    {
      m68k_unused_8_w(address, data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_8(address, data);
      return;
    }
  }
}

void ctrl_io_write_word(unsigned int address, unsigned int data)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        io_68k_write((address >> 1) & 0x0F, data & 0xFF);
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x11:  /* BUSREQ */
    {
      gen_zbusreq_w((data >> 8) & 1, mm68k.cycleCount);
      return;
    }

    case 0x12:  /* RESET */
    {
      gen_zreset_w((data >> 8) & 1, mm68k.cycleCount);
      return;
    }

    case 0x30:  /* TIME */
    {
      cart.hw.time_w(address, data);
      return;
    }

    case 0x40:  /* TMSS */
    {
      if (config.tmss & 1)
      {
        gen_tmss_w(address & 3, data);
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

	#ifndef NO_SVP
    case 0x50:  /* SVP REGISTERS */
    {
      if (!(address & 0xFD))
      {
        svp->ssp1601.gr[SSP_XST].h = data;
        svp->ssp1601.gr[SSP_PM0].h |= 2;
        svp->ssp1601.emu_status &= ~SSP_WAIT_PM0;
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }
	#endif

    case 0x10:  /* MEMORY MODE */
    case 0x20:  /* MEGA-CD */
    case 0x41:  /* OS ROM */
    case 0x44:  /* RADICA */
    {
      m68k_unused_16_w (address, data);
      return;
    }
            
    default:  /* Invalid address */
    {
      m68k_lockup_w_16 (address, data);
      return;
    }
  }
}


/*--------------------------------------------------------------------------*/
/* VDP                                                                      */
/*--------------------------------------------------------------------------*/
unsigned int vdp_read_byte(unsigned int address)
{
  switch (address & 0xFD)
  {
    case 0x00:  /* DATA */
    {
      return (vdp_68k_data_r() >> 8);
    }

    case 0x01:  /* DATA */
    {
      return (vdp_68k_data_r() & 0xFF);
    }

    case 0x04:  /* CTRL */
    {
      return (((vdp_68k_ctrl_r(mm68k.cycleCount) >> 8) & 3) | (m68k_read_pcrelative_8(mm68k, mm68k.pc) & 0xFC));
    }

    case 0x05:  /* CTRL */
    {
      return (vdp_68k_ctrl_r(mm68k.cycleCount) & 0xFF);
    }

    case 0x08:  /* HVC */
    case 0x0C:
    {
      return (vdp_hvc_r(mm68k.cycleCount) >> 8);
    }

    case 0x09:  /* HVC */
    case 0x0D:
    {
      return (vdp_hvc_r(mm68k.cycleCount) & 0xFF);
    }

    case 0x18:  /* Unused */
    case 0x19:
    case 0x1C:
    case 0x1D:
    {
      return m68k_read_bus_8(address);
    }

    default:    /* Invalid address */
    {
      return m68k_lockup_r_8(address);
    }
  }
}

unsigned int vdp_read_word(unsigned int address)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* DATA */
    {
      return vdp_68k_data_r();
    }

    case 0x04:  /* CTRL */
    {
      return ((vdp_68k_ctrl_r(mm68k.cycleCount) & 0x3FF) | (m68k_read_pcrelative_16(mm68k, mm68k.pc) & 0xFC00));
    }

    case 0x08:  /* HVC */
    case 0x0C:
    {
      return vdp_hvc_r(mm68k.cycleCount);
    }

    case 0x18:  /* Unused */
    case 0x1C:
    {
      return m68k_read_bus_16(address);
    }

    default:    /* Invalid address */
    {
      return m68k_lockup_r_16(address);
    }
  }
}

void vdp_write_byte(unsigned int address, unsigned int data)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* Data port */
    {
      vdp_68k_data_w(data << 8 | data);
      return;
    }

    case 0x04:  /* Control port */
    {
      vdp_68k_ctrl_w(data << 8 | data);
      return;
    }

    case 0x10:  /* PSG */
    case 0x14:
    {
      if (address & 1)
      {
        psg_write(mm68k.cycleCount, data);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x18: /* Unused */
    {
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x1C:  /* TEST register */
    {
      vdp_test_w(data << 8 | data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_8(address, data);
      return;
    }
  }
}

void vdp_write_word(unsigned int address, unsigned int data)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* DATA */
    {
      vdp_68k_data_w(data);
      return;
    }

    case 0x04:  /* CTRL */
    {
      vdp_68k_ctrl_w(data);
      return;
    }

    case 0x10:  /* PSG */
    case 0x14:
    {
      psg_write(mm68k.cycleCount, data & 0xFF);
      return;
    }

    case 0x18:  /* Unused */
    {
      m68k_unused_16_w(address, data);
      return;
    }
    
    case 0x1C:  /* Test register */
    {
      vdp_test_w(data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_16 (address, data);
      return;
    }
  }
}


/******* PICO ************************************************/
#ifndef NO_SYSTEM_PICO
unsigned int pico_read_byte(unsigned int address)
{
  /* PICO */
  switch (address & 0xFF)
  {
    case 0x01:  /* VERSION register */
    {
      return 0x40;
    }

    case 0x03:  /* IO register */
    {
      unsigned int retval = 0xFF;
      if (input.pad[0] & INPUT_B)     retval &= ~0x10;
      if (input.pad[0] & INPUT_A)     retval &= ~0x80;
      if (input.pad[0] & INPUT_UP)    retval &= ~0x01;
      if (input.pad[0] & INPUT_DOWN)  retval &= ~0x02;
      if (input.pad[0] & INPUT_LEFT)  retval &= ~0x04;
      if (input.pad[0] & INPUT_RIGHT) retval &= ~0x08;
      retval &= ~0x20;
      retval &= ~0x40;
      return retval;
    }

    case 0x05:  /* MSB PEN X coordinate */
    {
      return (input.analog[0][0] >> 8);
    }

    case 0x07:  /* LSB PEN X coordinate */
    {
      return (input.analog[0][0] & 0xFF);
    }

    case 0x09:  /* MSB PEN Y coordinate */
    {
      return (input.analog[0][1] >> 8);
    }

    case 0x0B:  /* LSB PEN Y coordinate */
    {
      return (input.analog[0][1] & 0xFF);
    }

    case 0x0D:  /* PAGE register (TODO) */
    {
      return pico_page[pico_current];
    }

    case 0x10:  /* PCM registers (TODO) */
    {
      return 0x80;
    }

    default:
    {
      return m68k_read_bus_8(address);
    }
  }
}

unsigned int pico_read_word(unsigned int address)
{
  return (pico_read_byte(address | 1) | (m68k_read_bus_8(address) << 8));
}
#endif

// debug hooks

const char *m68KAddrToStr(M68KCPU &cpu, unsigned addr)
{
	unsigned idx = (addr>>16)&0xff;
	#ifndef NO_SCD
	if(sCD.isActive)
	{
		if(&cpu == &mm68k)
		{
			switch(idx)
			{
				case 0x0: return "M:ROM(1)";
				case 0x1: return "M:ROM(2)";
				case 0x2: return "M:PRG(1)";
				case 0x3: return "M:PRG(2)";
				case 0x20: return "M:WORD(1)";
				case 0x21: return "M:WORD(2)";
				case 0x22: return "M:WORD(3)";
				case 0x23: return "M:WORD(4)";
			}
		}
		else
		{
			switch(idx)
			{
				case 0x0: return "S:PRG(1)";
				case 0x1: return "S:PRG(2)";
				case 0x2: return "S:PRG(3)";
				case 0x3: return "S:PRG(4)";
				case 0x4: return "S:PRG(5)";
				case 0x5: return "S:PRG(6)";
				case 0x6: return "S:PRG(7)";
				case 0x7: return "S:PRG(8)";
			}
		}
	}
	#endif
	return "";
}

static bool isVerboseCPUWrite(M68KCPU &cpu, unsigned addr)
{
	unsigned idx = (addr>>16)&0xff;
	#ifndef NO_SCD
	if(sCD.isActive)
	{
		if(&cpu == &mm68k)
		{
			switch(idx)
			{
				//case 0x0: return 0;
				//case 0x1: return 0;
				/*case 0x2: return 1;
				case 0x3: return 1;
				case 0x20: return 1;
				case 0x21: return 1;
				case 0x22: return 1;
				case 0x23: return 1;*/
			}
		}
		if(&cpu == &sCD.cpu)
		{
			switch(idx)
			{
				//case 0x0 ... 0x7: return 1;
				//case 0x8 ... 0xB: return 1;
			}
		}
	}
	#endif
	return 0;
}

static bool isVerboseCPURead(M68KCPU &cpu, unsigned addr)
{
	unsigned idx = (addr>>16)&0xff;
	#ifndef NO_SCD
	if(sCD.isActive)
	{
		if(&cpu == &mm68k)
		{
			switch(idx)
			{
				//case 0x0: return 0;
				//case 0x1: return 0;
				//case 0x2: return 1;
				//case 0x3: return 1;
				//case 0x20: return 0;
				//case 0x21: return 0;
				//case 0x22: return 0;
				//case 0x23: return 0;
			}
		}
	}
	#endif
	return 0;
}

void m68k_read_immediate_16_hook(M68KCPU &cpu, unsigned addr)
{
	unsigned mapIdx = ((addr)>>16)&0xff;
	if(isVerboseCPURead(cpu, addr))
	//if(cpu.id() == 0 && mapIdx < 0xFF)
		logMsg("read im 16: %s:0x%X, real %p+0x%X", m68KAddrToStr(cpu, addr), addr, cpu.memory_map[mapIdx].base, addr & 0xffff);
}

void m68k_read_immediate_32_hook(M68KCPU &cpu, unsigned addr)
{

}

void m68k_read_pcrelative_8_hook(M68KCPU &cpu, unsigned addr)
{
	unsigned mapIdx = ((addr)>>16)&0xff;
	if(isVerboseCPURead(cpu, addr))
		logMsg("read rel 8: %s:0x%X, real %p+0x%X", m68KAddrToStr(cpu, addr), addr, cpu.memory_map[mapIdx].base, addr & 0xffff);
}

void m68ki_read_8_hook(M68KCPU &cpu, unsigned address, const _m68k_memory_map *map)
{
	if(isVerboseCPURead(cpu, address))
	  logMsg("read 8: %s:0x%X, real %p+0x%X", m68KAddrToStr(cpu, address), address, map->base, address & 0xffff);
}

void m68ki_read_16_hook(M68KCPU &cpu, unsigned address, const _m68k_memory_map *map)
{
	if(isVerboseCPURead(cpu, address))
		logMsg("read 16: %s:0x%X, real %p+0x%X", m68KAddrToStr(cpu, address), address, map->base, address & 0xffff);
}

void m68ki_read_32_hook(M68KCPU &cpu, unsigned address, const _m68k_memory_map *map)
{
	if(isVerboseCPURead(cpu, address))
 		logMsg("read 32: %s:0x%X, real %p+0x%X", m68KAddrToStr(cpu, address), address, map->base, address & 0xffff);
}

void m68ki_write_8_hook(M68KCPU &cpu, unsigned address, const _m68k_memory_map *map, unsigned value)
{
	if(isVerboseCPUWrite(cpu, address))
 		logMsg("write 8: %s:0x%X with 0x%X, real %p+0x%X", m68KAddrToStr(cpu, address), address, value, map->base, address & 0xffff);
}

void m68ki_write_16_hook(M68KCPU &cpu, unsigned address, const _m68k_memory_map *map, unsigned value)
{
	if(isVerboseCPUWrite(cpu, address))
		logMsg("write 16: %s:0x%X with 0x%X, real %p+0x%X", m68KAddrToStr(cpu, address), address, value, map->base, address & 0xffff);
}

void m68ki_write_32_hook(M68KCPU &cpu, unsigned address, const _m68k_memory_map *map, unsigned value)
{
	if(isVerboseCPUWrite(cpu, address))
	  logMsg("write 32: %s:0x%X with 0x%X, real %p+0x%X", m68KAddrToStr(cpu, address), address, value, map->base, address & 0xffff);
}
