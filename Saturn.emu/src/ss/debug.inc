/******************************************************************************/
/* Mednafen Sega Saturn Emulation Module                                      */
/******************************************************************************/
/* debug.inc:
**  Copyright (C) 2015-2019 Mednafen Team
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifdef WANT_DEBUGGER
static struct DBGS
{
 enum { NUMBT = 24 };

 struct
 {
  uint32 from;
  uint32 to;
  uint32 branch_count;
  int exception;	// -1 on no exception.
  unsigned vecnum;
  bool valid;
 } BTEntries[2][NUMBT];

 bool BTEnabled;
 unsigned BTIndex[2];

 struct SS_BPOINT
 {
  uint32 A[2];
 };

 std::vector<SS_BPOINT> BreakPointsRead, BreakPointsWrite, BreakPointsPC;

 void (*CPUHook)(uint32, bool);
 bool CPUHookContinuous;

 bool FoundBPoint;

 uint32 CurPC[2];

 unsigned ActiveCPU;

 sscpu_timestamp_t eff_ts;
 bool inss;
} DBG;

static MDFN_COLD MDFN_NOWARN_UNUSED void DBG_Break(void)
{
 DBG.FoundBPoint = true;
}

static MDFN_COLD void DBG_CheckReadBP(unsigned len, uint32 addr)
{
 for(auto const& bp : DBG.BreakPointsRead)
 {
  for(uint32 ta = addr; ta != (addr + len); ta++)	// TODO: Optimize
  {
   if(ta >= bp.A[0] && ta <= bp.A[1])
   {
    DBG.FoundBPoint = true;
    return;
   }
  }
 }
}

static MDFN_COLD void DBG_CheckWriteBP(unsigned len, uint32 addr)
{
 for(auto const& bp : DBG.BreakPointsWrite)
 {
  for(uint32 ta = addr; ta != (addr + len); ta++)	// TODO: Optimize
  {
   if(ta >= bp.A[0] && ta <= bp.A[1])
   {
    DBG.FoundBPoint = true;
    return;
   }
  }
 }
}

enum
{
 ASPACE_PHYSICAL = 0,
 ASPACE_BACKUP,
 ASPACE_WORKRAML,
 ASPACE_WORKRAMH,
 ASPACE_SCSPRAM,
 ASPACE_SCSPMPROG,
 ASPACE_SCSPTEMP,
 ASPACE_SCSPMEMS,
 ASPACE_DSPPROG,
 ASPACE_VDP1VRAM,
 ASPACE_VDP1FB0,
 ASPACE_VDP1FB1,
 ASPACE_VDP2VRAM
};

template<unsigned id>
static MDFN_COLD void GetAddressSpaceBytes(const char *name, uint32 Address, uint32 Length, uint8 *Buffer)
{
 while(MDFN_LIKELY(Length--))
 {
  switch(id)
  {
   default:
	break;

   case ASPACE_PHYSICAL:
	//TODO:
	//Address &= 0x07FFFFFF;
	//*Buffer = SH7095_BusPeek<uint8>(Address);
	break;

   case ASPACE_BACKUP:
	Address &= 0x7FFF;
	*Buffer = BackupRAM[Address];
	break;

   case ASPACE_WORKRAML:
	Address &= 0xFFFFF;
	*Buffer = ne16_rbo_be<uint8>(WorkRAML, Address);
	break;

   case ASPACE_WORKRAMH:
	Address &= 0xFFFFF;
	*Buffer = ne16_rbo_be<uint8>(WorkRAMH, Address);
	break;

   case ASPACE_SCSPRAM:
	Address &= 0x7FFFF;
	*Buffer = SOUND_PeekRAM(Address);
	break;

   case ASPACE_SCSPMPROG:
	Address &= 0x3FF;
	*Buffer = SOUND_PeekMPROG(Address >> 3) >> (((Address & 0x7) ^ 0x7) << 3);
	break;

   case ASPACE_SCSPMEMS:
	Address &= 0x7F;
	*Buffer = SOUND_PeekMEMS(Address >> 2) >> (((Address & 0x3) ^ 0x3) << 3);
	break;

   case ASPACE_SCSPTEMP:
	Address &= 0x1FF;
	*Buffer = SOUND_PeekTEMPRel(Address >> 2) >> (((Address & 0x3) ^ 0x3) << 3);
	break;

   case ASPACE_DSPPROG:
	Address &= 0x3FF;
	*Buffer = SCU_DSP_PeekProgRAM(Address >> 2) >> (((Address & 0x3) ^ 0x3) << 3);
	break;

   case ASPACE_VDP1VRAM:
	Address &= 0x7FFFF;
	*Buffer = VDP1::PeekVRAM(Address);
	break;

   case ASPACE_VDP1FB0:
   case ASPACE_VDP1FB1:
	Address &= 0x3FFFF;
	*Buffer = VDP1::PeekFB(id == ASPACE_VDP1FB1, Address);
	break;

   case ASPACE_VDP2VRAM:
	Address &= 0x7FFFF;
	*Buffer = VDP2::PeekVRAM(Address);
	break;
  }
  Address++;
  Buffer++;
 }
}

template<typename T, uint64 N, typename PEFT, typename POFT>
static INLINE void PutHelper(PEFT peekfunc, POFT pokefunc, uint32 address, const uint8* buffer)
{
 static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Wrong type.");
 uint32 stl2;

 if(sizeof(T) == 8)
  stl2 = 3;
 else if(sizeof(T) == 4)
  stl2 = 2;
 else if(sizeof(T) == 2)
  stl2 = 1;
 else if(sizeof(T) == 1)
  stl2 = 0;

 address &= (sizeof(T) * N) - 1;
 //
 uint32 offs = address >> stl2;
 uint8 shift = (((address & (sizeof(T) - 1)) ^ (sizeof(T) - 1)) << 3);
 T tmp;

 tmp = peekfunc(offs);
 tmp &= ~((T)0xFF << shift);
 tmp |= (T)*buffer << shift;
 pokefunc(offs, tmp);
}

template<unsigned id>
static MDFN_COLD void PutAddressSpaceBytes(const char *name, uint32 Address, uint32 Length, uint32 Granularity, bool hl, const uint8 *Buffer)
{
 while(MDFN_LIKELY(Length--))
 {
  switch(id)
  {
   default:
	break;

   case ASPACE_PHYSICAL:
	// TODO:
	//Address &= 0x07FFFFFF;
	//*Buffer = SH7095_BusPoke<uint8>(Address, *Buffer);
	break;

   case ASPACE_BACKUP:
	Address &= 0x7FFF;
	BackupRAM[Address] = *Buffer;
	BackupRAM_Dirty = true;
	break;

   case ASPACE_WORKRAML:
	Address &= 0xFFFFF;
	ne16_wbo_be<uint8>(WorkRAML, Address, *Buffer);
	break;

   case ASPACE_WORKRAMH:
	Address &= 0xFFFFF;
	ne16_wbo_be<uint8>(WorkRAMH, Address, *Buffer);
	break;

   case ASPACE_SCSPRAM:
	Address &= 0x7FFFF;
	SOUND_PokeRAM(Address, *Buffer);
	break;

   case ASPACE_SCSPMPROG:
	PutHelper<uint64, 0x80>(SOUND_PeekMPROG, SOUND_PokeMPROG, Address, Buffer);
	break;

   case ASPACE_SCSPMEMS:
	PutHelper<uint32, 0x20>(SOUND_PeekMEMS, SOUND_PokeMEMS, Address, Buffer);
	break;

   case ASPACE_SCSPTEMP:
	PutHelper<uint32, 0x80>(SOUND_PeekTEMPRel, SOUND_PokeTEMPRel, Address, Buffer);
	break;

   case ASPACE_DSPPROG:
	// TODO:
	break;

   case ASPACE_VDP1VRAM:
	Address &= 0x7FFFF;
	VDP1::PokeVRAM(Address, *Buffer);
	break;

   case ASPACE_VDP1FB0:
   case ASPACE_VDP1FB1:
	Address &= 0x3FFFF;
	VDP1::PokeFB(id == ASPACE_VDP1FB1, Address, *Buffer);
	break;

   case ASPACE_VDP2VRAM:
	Address &= 0x7FFFF;
	VDP2::PokeVRAM(Address, *Buffer);
	break;
  }
  Address++;
  Buffer++;
 }
}


static MDFN_COLD uint32 DBG_MemPeek(uint32 A, unsigned int bsize, bool hl, bool logical)
{
/*
 uint32 ret = 0;

 for(unsigned int i = 0; i < bsize; i++)
  ret |= CPU->PeekMem8(A + i) << (i * 8);

 return(ret);
*/
 return 0xAA;
}


static MDFN_COLD void DBG_FlushBreakPoints(int type)
{
 if(type == BPOINT_READ)
  DBG.BreakPointsRead.clear();
 else if(type == BPOINT_WRITE)
  DBG.BreakPointsWrite.clear();
 else if(type == BPOINT_PC)
  DBG.BreakPointsPC.clear();
}

static MDFN_COLD void DBG_AddBreakPoint(int type, unsigned int A1, unsigned int A2, bool logical)
{
 DBGS::SS_BPOINT tmp;

 tmp.A[0] = A1;
 tmp.A[1] = A2;

 if(type == BPOINT_READ)
  DBG.BreakPointsRead.push_back(tmp);
 else if(type == BPOINT_WRITE)
  DBG.BreakPointsWrite.push_back(tmp);
 else if(type == BPOINT_PC)
  DBG.BreakPointsPC.push_back(tmp);
}

static MDFN_COLD void DBG_SetCPUCallback(void (*callb)(uint32 PC, bool bpoint), bool continuous)
{
 DBG.CPUHook = callb;
 DBG.CPUHookContinuous = continuous;
}

static MDFN_COLD void DBG_EnableBranchTrace(bool enable)
{
 if((DBG.BTEnabled ^ enable) & DBG.BTEnabled)
 {
  for(unsigned which = 0; which < 2; which++)
   for(unsigned i = 0; i < DBGS::NUMBT; i++)
    DBG.BTEntries[which][i].valid = false;
 }

 DBG.BTEnabled = enable;
}

static MDFN_COLD void DBG_AddBranchTrace(unsigned which, uint32 to, int exception, unsigned vecnum = 0)
{
 const uint32 from = DBG.CurPC[which];
 auto *prevbt = &DBG.BTEntries[which][(DBG.BTIndex[which] + DBGS::NUMBT - 1) % DBGS::NUMBT];

 //if(BTEntries[(BTIndex - 1) & 0xF] == PC) return;

 if(prevbt->from == from && prevbt->to == to && prevbt->exception == exception && prevbt->branch_count < 0xFFFFFFFF && prevbt->valid)
  prevbt->branch_count++;
 else
 {
  auto& bte = DBG.BTEntries[which][DBG.BTIndex[which]];
  bte.from = from;
  bte.to = to;
  bte.exception = exception;
  bte.vecnum = vecnum;
  bte.branch_count = 1;
  bte.valid = true;

  DBG.BTIndex[which] = (DBG.BTIndex[which] + 1) % DBGS::NUMBT;
 }
}

static MDFN_COLD std::vector<BranchTraceResult> DBG_GetBranchTrace(void)
{
 std::vector<BranchTraceResult> ret;
 BranchTraceResult tmp;

 for(unsigned x = 0; x < DBGS::NUMBT; x++)
 {
  char estr[32];
  const auto* bt = &DBG.BTEntries[DBG.ActiveCPU][(x + DBG.BTIndex[DBG.ActiveCPU]) % DBGS::NUMBT];

  if(!bt->valid)
   continue;

  tmp.count = bt->branch_count;
  trio_snprintf(tmp.from, sizeof(tmp.from), "%08x", bt->from);
  trio_snprintf(tmp.to, sizeof(tmp.to), "%08x", bt->to);

  trio_snprintf(estr, sizeof(estr), "UNK");

  switch(bt->exception)
  {
   case -1:
	estr[0] = 0;
	break;

   case SH7095::EXCEPTION_POWERON:
	trio_snprintf(estr, sizeof(estr), "PRST");
	break;

   case SH7095::EXCEPTION_RESET:
	trio_snprintf(estr, sizeof(estr), "MRST");
	break;

   case SH7095::EXCEPTION_ILLINSTR:
	trio_snprintf(estr, sizeof(estr), "ILL");
	break;

   case SH7095::EXCEPTION_ILLSLOT:
	trio_snprintf(estr, sizeof(estr), "ILLS");
	break;

   case SH7095::EXCEPTION_CPUADDR:
	trio_snprintf(estr, sizeof(estr), "CPUA");
	break;

   case SH7095::EXCEPTION_DMAADDR:
	trio_snprintf(estr, sizeof(estr), "DMAA");
	break;

   case SH7095::EXCEPTION_NMI:
	trio_snprintf(estr, sizeof(estr), "NMI");
	break;

   case SH7095::EXCEPTION_BREAK:
	trio_snprintf(estr, sizeof(estr), "BRK");
	break;

   case SH7095::EXCEPTION_TRAP:
	trio_snprintf(estr, sizeof(estr), "TRAP%02X", bt->vecnum);
	break;

   case SH7095::EXCEPTION_INT:
	trio_snprintf(estr, sizeof(estr), "INT%02X", bt->vecnum);
	break;
  }

  trio_snprintf(tmp.code, sizeof(tmp.code), "%s", estr);

  ret.push_back(tmp);
 }
 return(ret);
}

static INLINE void DBG_SetEffTS(const sscpu_timestamp_t eff_ts)
{
 DBG.eff_ts = eff_ts;
}

static INLINE bool DBG_InSlaveStep(void)
{
 return DBG.inss;
}

template<unsigned which>
static MDFN_COLD void DBG_CPUHandler(void)
{
 const uint32 PC = CPU[which].GetRegister(SH7095::GSREG_PC_ID, NULL, 0);

 if(which != DBG.ActiveCPU)
 {
  DBG.CurPC[which] = PC;
  return;
 }

#if 0
 if(LogFunc)
 {
  static const uint32 addr_mask[8] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF, 0x1FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
  uint32 tpc = PC & addr_mask[PC >> 29];;

  if(MDFN_UNLIKELY(tpc <= 0xC0))
  {
   if(tpc == 0xA0 || tpc == 0xB0 || tpc == 0xC0)
   {
    const uint32 function = CPU->GetRegister(PS_CPU::GSREG_GPR + 9, NULL, 0);

    if(tpc != 0xB0 || function != 0x17)
    {
     char tmp[64];
     trio_snprintf(tmp, sizeof(tmp), "0x%02x:0x%02x", PC & 0xFF, function);
     LogFunc("BIOS", tmp);
    }
   }
  }
 }
#endif

 for(auto& bp : DBG.BreakPointsPC)
 {
  if(PC >= bp.A[0] && PC <= bp.A[1])
  {
   DBG.FoundBPoint = true;
   break;
  }
 }

 CPU[which].CheckRWBreakpoints(DBG_CheckReadBP, DBG_CheckWriteBP);
 //CPU->CheckBreakpoints(CheckCPUBPCallB, CPU->PeekMem32(PC));

 DBG.CPUHookContinuous |= DBG.FoundBPoint;

 if(DBG.CPUHookContinuous && DBG.CPUHook)
 {
  DBG.inss = which;
  ForceEventUpdates(DBG.eff_ts);
  DBG.CPUHook(PC, DBG.FoundBPoint);
  DBG.inss = false;
 }

 DBG.FoundBPoint = false;
 //
 //
 // No, no, this isn't right for dual-CPUs(FIXME):
 DBG.CurPC[which] = CPU[which].GetRegister(SH7095::GSREG_PC_ID, NULL, 0);
}


static MDFN_COLD bool DBG_NeedCPUHooks(void)
{
 return DBG.BTEnabled || DBG.CPUHook || DBG.BreakPointsPC.size() || DBG.BreakPointsRead.size() || DBG.BreakPointsWrite.size();
}

// TODO: Standard peek functions
static MDFN_COLD uint16 DBG_DisPeek16(uint32 A)
{
 return *(uint16*)(SH7095_FastMap[A >> SH7095_EXT_MAP_GRAN_BITS] + A);
}

static MDFN_COLD uint32 DBG_DisPeek32(uint32 A)
{
 uint32 ret;

 ret = *(uint16*)(SH7095_FastMap[A >> SH7095_EXT_MAP_GRAN_BITS] + A) << 16;
 A |= 2;
 ret |= *(uint16*)(SH7095_FastMap[A >> SH7095_EXT_MAP_GRAN_BITS] + A) << 0;

 return ret;
}

static MDFN_COLD void DBG_Disassemble(uint32 &A, uint32 SpecialA, char *TextBuf)
{
 if(A & 0x1)
 {
  strncpy(TextBuf, "UNALIGNED", 256);
  A &= ~0x1;
 }
 else
 {
  uint16 instr;

  if(A == CPU[DBG.ActiveCPU].GetRegister(SH7095::GSREG_PC_ID, NULL, 0))
   instr = CPU[DBG.ActiveCPU].GetRegister(SH7095::GSREG_PID, NULL, 0);
  else if(A == CPU[DBG.ActiveCPU].GetRegister(SH7095::GSREG_PC_IF, NULL, 0))
   instr = CPU[DBG.ActiveCPU].GetRegister(SH7095::GSREG_PIF, NULL, 0);
  else
   instr = DBG_DisPeek16(A);

  SH7095::Disassemble(instr, A + 4, TextBuf, DBG_DisPeek16, DBG_DisPeek32);
 }

 char* tlc = TextBuf;

 while((uint8)*tlc != 0)
 {
  *tlc = tolower(*tlc);
  tlc++;
 }

 A += 2;
}

static MDFN_COLD void DBG_ToggleSyntax(void)
{
 // Right, "syntax". ;)
 DBG.ActiveCPU = !DBG.ActiveCPU;
}
//
//
//

static const RegType DBG_Regs_CPU[] =
{
 { SH7095::GSREG_PC_ID, "PC", "PC (Effective)", 4 },
 { SH7095::GSREG_RPC, "RPC", "PC (Real)", 4 },

 { SH7095::GSREG_PID, "PID", "Pipeline ID Buffer", 4 },
 { SH7095::GSREG_PIF, "PIF", "Pipeline IF Buffer", 4 },

 { SH7095::GSREG_EP, "EP", "Exception Pending", 4 },

 { SH7095::GSREG_R0, "R0", "R0", 4 },
 { SH7095::GSREG_R1, "R1", "R1",  4 },
 { SH7095::GSREG_R2, "R2", "R2",  4 },
 { SH7095::GSREG_R3, "R3", "R3",  4 },
 { SH7095::GSREG_R4, "R4", "R4",  4 },
 { SH7095::GSREG_R5, "R5", "R5",  4 },
 { SH7095::GSREG_R6, "R6", "R6",  4 },
 { SH7095::GSREG_R7, "R7", "R7",  4 },
 { SH7095::GSREG_R8, "R8", "R8",  4 },
 { SH7095::GSREG_R9, "R9", "R9",  4 },
 { SH7095::GSREG_R10, "R10", "R10",  4 },
 { SH7095::GSREG_R11, "R11", "R11",  4 },
 { SH7095::GSREG_R12, "R12", "R12",  4 },
 { SH7095::GSREG_R13, "R13", "R13",  4 },
 { SH7095::GSREG_R14, "R14", "R14",  4 },
 { SH7095::GSREG_R15, "R15", "R15/Stack Pointer", 4 },

 { 0, "------", "", 0xFFFF },

 { SH7095::GSREG_SR, "SR", "Status Register", 4 },
 { SH7095::GSREG_GBR, "GBR", "Global Base Register", 4 },
 { SH7095::GSREG_VBR, "VBR", "Vector Base Register", 4 },

 { 0, "------", "", 0xFFFF },

 { SH7095::GSREG_MACH, "MACH", "Multiply-and-Accumulate High", 4 },
 { SH7095::GSREG_MACL, "MACL", "Multiply-and-Accumulate Low", 4 },

 { 0, "------", "", 0xFFFF },

 { SH7095::GSREG_PR, "PR", "Procedure Register", 4 },

 { 0, "------", "", 0xFFFF },

 { SH7095::GSREG_NMIL, "NMIL", "NMI Level(Input)", 1 },
 { SH7095::GSREG_IRL, "IRL", "Interrupt Level(Input)", 1 },
 { SH7095::GSREG_IPRA, "IPRA", "IPRA", 2 },
 { SH7095::GSREG_IPRB, "IPRB", "IPRB", 2 },
 { SH7095::GSREG_VCRWDT, "VCRWDT", "VCRWDT", 2 },
 { SH7095::GSREG_VCRA, "VCRA", "VCRA", 2 },
 { SH7095::GSREG_VCRB, "VCRB", "VCRB", 2 },
 { SH7095::GSREG_VCRC, "VCRC", "VCRC", 2 },
 { SH7095::GSREG_VCRD, "VCRD", "VCRD", 2 },
 { SH7095::GSREG_ICR, "ICR", "ICR", 2 },

 { 0, "------", "", 0xFFFF },

 { SH7095::GSREG_DVSR, "DVSR", "Divisor", 4 },
 { SH7095::GSREG_DVDNT, "DVDNT", "DVDNT", 4 },
 { SH7095::GSREG_DVDNTH, "DVDNTH", "DVDNTH", 4 },
 { SH7095::GSREG_DVDNTL, "DVDNTL", "DVDNTL", 4 },
 { SH7095::GSREG_DVDNTHS, "DVDNTHS", "DVDNTH Shadow", 4 },
 { SH7095::GSREG_DVDNTLS, "DVDNTLS", "DVDNTL Shadow", 4 },
 { SH7095::GSREG_VCRDIV, "VCRDIV", "VCRDIV", 2 },
 { SH7095::GSREG_DVCR, "DVCR", "DVCR", 1 },

 { 0, "------", "", 0xFFFF },

 { SH7095::GSREG_WTCSR, "WTCSR", "WTCSR", 1 },
 { SH7095::GSREG_WTCSRM, "WTCSRM", "WTCSRM", 1 },
 { SH7095::GSREG_WTCNT, "WTCNT", "WTCNT", 1 },
 { SH7095::GSREG_RSTCSR, "RSTCSR", "RSTCSR", 1 },
 { SH7095::GSREG_RSTCSRM, "RSTCSRM", "RSTCSRM", 1 },

 { 0, "", "", 0 },
};

static const RegType DBG_Regs_CPUE_SCSP[] =
{
 { SH7095::GSREG_DMAOR, "DMAOR", "DMAOR", 1 },
 { SH7095::GSREG_DMAORM, "DMAORM", "DMAORM", 1 },

 { 0, "--DMACH0:--", "", 0xFFFF },
 { SH7095::GSREG_DMA0_SAR, "SAR", "SAR", 4 },
 { SH7095::GSREG_DMA0_DAR, "DAR", "DAR", 4 },
 { SH7095::GSREG_DMA0_TCR, "TCR", "TCR", 4 },
 { SH7095::GSREG_DMA0_CHCR, "CHCR", "CHCR", 2 },
 { SH7095::GSREG_DMA0_CHCRM, "CHCRM", "CHCRM", 2 },
 { SH7095::GSREG_DMA0_VCR, "VCR", "VCR", 1 },
 { SH7095::GSREG_DMA0_DRCR, "DRCR", "DRCR", 1 },

 { 0, "--DMACH1:--", "", 0xFFFF },
 { SH7095::GSREG_DMA1_SAR, "SAR", "SAR", 4 },
 { SH7095::GSREG_DMA1_DAR, "DAR", "DAR", 4 },
 { SH7095::GSREG_DMA1_TCR, "TCR", "TCR", 4 },
 { SH7095::GSREG_DMA1_CHCR, "CHCR", "CHCR", 2 },
 { SH7095::GSREG_DMA1_CHCRM, "CHCRM", "CHCRM", 2 },
 { SH7095::GSREG_DMA1_VCR, "VCR", "VCR", 1 },
 { SH7095::GSREG_DMA1_DRCR, "DRCR", "DRCR", 1 },

 { 0, "-----------", "", 0xFFFF },

 { SH7095::GSREG_FRC, "FRC", "FRC", 2 },
 { SH7095::GSREG_OCR0, "OCRA", "OCRA", 2 },
 { SH7095::GSREG_OCR1, "OCRB", "OCRB", 2 },
 { SH7095::GSREG_FICR, "FICR", "FICR", 2 },
 { SH7095::GSREG_TIER, "TIER", "TIER", 1 },
 { SH7095::GSREG_FTCSR, "FTCSR", "FTCSR", 1 },
 { SH7095::GSREG_FTCSRM, "FTCSRM", "FTCSRM", 1 },
 { SH7095::GSREG_TCR, "TCR", "TCR", 1 },
 { SH7095::GSREG_TOCR, "TOCR", "TOCR", 1 },
 { SH7095::GSREG_RWT, "RWT", "R/W Temp", 1 },

 { 0, "-----------", "", 0xFFFF },

 { SH7095::GSREG_CCR, "CCR", "Cache Control Register", 1 },
 { SH7095::GSREG_SBYCR, "SBYCR", "Standby Control Register", 1 },

 { 0, "------", "", 0xFFFF },

 { (4 << 16) | SS_SCSP::GSREG_MVOL, "MVOL", "Master Volume", 0x100 | 4 },
 { (4 << 16) | SS_SCSP::GSREG_DAC18B, "DAC18B", "18-bit DAC Enable", 0x100 | 1 },
 { (4 << 16) | SS_SCSP::GSREG_MEM4MB, "Mem4Mb", "4Mb Memory Enable", 0x100 | 1 },
 { (4 << 16) | SS_SCSP::GSREG_MSLC, "MSLC", "Slot to Monitor", 0x100 | 5 },

 { (4 << 16) | SS_SCSP::GSREG_SCIEB, "SCIEB", "SCIEB", 2 },
 { (4 << 16) | SS_SCSP::GSREG_SCIPD, "SCIPD", "SCIPD", 2 },

 { (4 << 16) | SS_SCSP::GSREG_MCIEB, "MCIEB", "MCIEB", 2 },
 { (4 << 16) | SS_SCSP::GSREG_MCIPD, "MCIPD", "MCIPD", 2 },
/*
 { (6 << 16) | M68K::GSREG_PC, "PC", "PC", 4 },
 { (6 << 16) | M68K::GSREG_SR, "SR", "SR", 2 },
 { (6 << 16) | M68K::GSREG_SSP, "SSP", "SSP", 4 },
 { (6 << 16) | M68K::GSREG_USP, "USP", "USP", 4 },
*/
 { (4 << 16) | SS_SCSP::GSREG_RBC, "RBC", "DSP Ringbuffer Control", 0x100 | 9 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG0, "EFREG0", "DSP Effect Register 0", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG1, "EFREG1", "DSP Effect Register 1", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG2, "EFREG2", "DSP Effect Register 2", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG3, "EFREG3", "DSP Effect Register 3", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG4, "EFREG4", "DSP Effect Register 4", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG5, "EFREG5", "DSP Effect Register 5", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG6, "EFREG6", "DSP Effect Register 6", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG7, "EFREG7", "DSP Effect Register 7", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG8, "EFREG8", "DSP Effect Register 8", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREG9, "EFREG9", "DSP Effect Register 9", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREGA, "EFREGA", "DSP Effect Register A", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREGB, "EFREGB", "DSP Effect Register B", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREGC, "EFREGC", "DSP Effect Register C", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREGD, "EFREGD", "DSP Effect Register D", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREGE, "EFREGE", "DSP Effect Register E", 2 },
 { (4 << 16) | SS_SCSP::GSREG_EFREGF, "EFREGF", "DSP Effect Register F", 2 },

 { 0, "", "", 0 },
};

static MDFN_COLD uint32 GetRegister(const unsigned id, char* const special, const uint32 special_len)
{
 switch(id >> 16)
 {
  case 0: return CPU[DBG.ActiveCPU].GetRegister((uint16)id, special, special_len);
  case 1: return CDB_GetRegister((uint16)id, special, special_len);
  case 2: return VDP1::GetRegister((uint16)id, special, special_len);
  case 3: return VDP2::GetRegister((uint16)id, special, special_len);
  case 4: return SOUND_GetSCSPRegister((uint16)id, special, special_len);
  case 5: return SCU_GetRegister((uint16)id, special, special_len);
  case 6: return SOUND_GetM68KRegister((uint16)id, special, special_len);
 }

 return 0;
}

static MDFN_COLD void SetRegister(const unsigned id, const uint32 value)
{
 switch(id >> 16)
 {
  case 0: CPU[DBG.ActiveCPU].SetRegister((uint16)id, value); break;
  case 1: CDB_SetRegister((uint16)id, value); break;
  case 2: VDP1::SetRegister((uint16)id, value); break;
  case 3: VDP2::SetRegister((uint16)id, value); break;
  case 4: SOUND_SetSCSPRegister((uint16)id, value); break;
  case 5: SCU_SetRegister((uint16)id, value); break;
  case 6: SOUND_SetM68KRegister((uint16)id, value); break;
 }
}

static const RegGroupType DBG_RegGroup_CPU =
{
 NULL,
 DBG_Regs_CPU,
 GetRegister,
 SetRegister
};

static const RegGroupType DBG_RegGroup_CPUE_SCSP =
{
 NULL,
 DBG_Regs_CPUE_SCSP,
 GetRegister,
 SetRegister
};


static const RegType DBG_Regs_SCU_CDB_VDP1[] =
{
 { (5 << 16) | SCU_GSREG_ILEVEL, "ILevel", "IRL Output to SH2-M", 1 },
 { (5 << 16) | SCU_GSREG_IVEC, "IVec", "Pending IRQ Vector Number", 1 },

 { 0, "------", "", 0xFFFF },

 { (5 << 16) | SCU_GSREG_IASSERTED, "IAss", "Status of Input IRQ Lines", 4 },
 { (5 << 16) | SCU_GSREG_IPENDING, "IPend", "Pending IRQs", 4 },
 { (5 << 16) | SCU_GSREG_IMASK, "IMask", "IRQ Mask", 2 },

 { 0, "------", "", 0xFFFF },

 { (5 << 16) | SCU_GSREG_D0MD, "D0MD", "D0MD", 4 },
 { (5 << 16) | SCU_GSREG_D1MD, "D1MD", "D1MD", 4 },
 { (5 << 16) | SCU_GSREG_D2MD, "D2MD", "D2MD", 4 },

 { 0, "------", "", 0xFFFF },

 { (5 << 16) | SCU_GSREG_ASR0_CS0, "ASR0_CS0", "A-bus CS0 Config", 2 },
 { (5 << 16) | SCU_GSREG_ASR0_CS1, "ASR0_CS1", "A-bus CS1 Config", 2 },
 { (5 << 16) | SCU_GSREG_ASR1_CS2, "ASR1_CS2", "A-bus CS2 Config", 2 },
 { (5 << 16) | SCU_GSREG_ASR1_CSD, "ASR1_CSD", "A-bus Dummy Config", 2 },

 { (5 << 16) | SCU_GSREG_AREF, "AREF", "A-bus Refresh Config", 1 },

 { (5 << 16) | SCU_GSREG_RSEL, "RSEL", "SDRAM Size Config", 0x100 | 1 },

 { 0, "------", "", 0xFFFF },

 { (5 << 16) | SCU_GSREG_TENBL, "TENBL", "Timers Enable", 0x100 | 1 },

 { (5 << 16) | SCU_GSREG_T0CNT, "T0CNT", "Timer0 Counter", 0x100 | 10 },
 { (5 << 16) | SCU_GSREG_T0CMP, "T0CMP", "Timer0 Compare Value", 0x100 | 10 },
 { (5 << 16) | SCU_GSREG_T0MET, "T0MET", "Timer0 Met", 0x100 | 1 },

 { (5 << 16) | SCU_GSREG_T1RLV, "T1RLV", "Timer1 Reload Value", 0x100 | 9 },
 { (5 << 16) | SCU_GSREG_T1CNT, "T1CNT", "Timer1 Counter", 0x100 | 9 },
 { (5 << 16) | SCU_GSREG_T1MOD, "T1MOD", "Timer1 Mode", 0x100 | 1 },
 { (5 << 16) | SCU_GSREG_T1MET, "T1MET", "Timer1 Met", 0x100 | 1 },

 { 0, "--DSP:--", "", 0xFFFF },

 { (5 << 16) | SCU_GSREG_DSP_EXEC, "EXEC", "Executing", 0x100 | 1 },
 { (5 << 16) | SCU_GSREG_DSP_PAUSE, "PAUSE", "Paused", 0x100 | 1 },
 { (5 << 16) | SCU_GSREG_DSP_PC, "PC", "Program Counter", 1 },
 { (5 << 16) | SCU_GSREG_DSP_END, "END", "End Flag", 0x100 | 1 },

 { 0, "------", "", 0xFFFF },

 { (1 << 16) | CDB_GSREG_HIRQ, "HIRQ", "HIRQ", 2 },
 { (1 << 16) | CDB_GSREG_HIRQ_MASK, "HIRQM", "HIRQ Mask", 2 },

 { (1 << 16) | CDB_GSREG_CDATA0, "CDATA0", "Command Data 0", 2 },
 { (1 << 16) | CDB_GSREG_CDATA1, "CDATA1", "Command Data 1", 2 },
 { (1 << 16) | CDB_GSREG_CDATA2, "CDATA2", "Command Data 2", 2 },
 { (1 << 16) | CDB_GSREG_CDATA3, "CDATA3", "Command Data 3", 2 },

 { (1 << 16) | CDB_GSREG_RESULT0, "RES0", "Result Data 0", 2 },
 { (1 << 16) | CDB_GSREG_RESULT1, "RES1", "Result Data 1", 2 },
 { (1 << 16) | CDB_GSREG_RESULT2, "RES2", "Result Data 2", 2 },
 { (1 << 16) | CDB_GSREG_RESULT3, "RES3", "Result Data 3", 2 },

 { 0, "------", "", 0xFFFF },

 { (2 << 16) | VDP1::GSREG_SYSCLIPX, "SClipX", "SysClipX", 2 },
 { (2 << 16) | VDP1::GSREG_SYSCLIPY, "SClipY", "SysClipY", 2 },
 { (2 << 16) | VDP1::GSREG_USERCLIPX0, "UClipX0", "UserClipX0", 2 },
 { (2 << 16) | VDP1::GSREG_USERCLIPY0, "UClipY0", "UserClipY0", 2 },
 { (2 << 16) | VDP1::GSREG_USERCLIPX1, "UClipX1", "UserClipX1", 2 },
 { (2 << 16) | VDP1::GSREG_USERCLIPY1, "UClipY1", "UserClipY1", 2 },
 { (2 << 16) | VDP1::GSREG_LOCALX, "LocalX", "LocalX", 2 },
 { (2 << 16) | VDP1::GSREG_LOCALY, "LocalY", "LocalY", 2 },

 { (2 << 16) | VDP1::GSREG_TVMR, "TVMR", "TVMR", 1 },
 { (2 << 16) | VDP1::GSREG_FBCR, "FBCR", "FBCR", 1 },
 { (2 << 16) | VDP1::GSREG_EWDR, "EWDR", "EWDR", 2 },
 { (2 << 16) | VDP1::GSREG_EWLR, "EWLR", "EWLR", 2 },
 { (2 << 16) | VDP1::GSREG_EWRR, "EWRR", "EWRR", 2 },

 { 0, "", "", 0 },
};


static const RegGroupType DBG_RegGroup_SCU_CDB_VDP1 =
{
 NULL,
 DBG_Regs_SCU_CDB_VDP1,
 GetRegister,
 SetRegister
};

//
//

static const RegType DBG_Regs_VDP2[] =
{
 { (3 << 16) | VDP2::GSREG_LINE, "Line", "Line", 2 },
 { (3 << 16) | VDP2::GSREG_DON, "DON", "Display On", 0x100 | 1 },
 { (3 << 16) | VDP2::GSREG_BM, "BM", "Border Mode", 0x100 | 1 },
 { (3 << 16) | VDP2::GSREG_IM, "IM", "Interlace Mode", 0x100 | 2 },
 { (3 << 16) | VDP2::GSREG_VRES, "VRES", "Vertical Resolution", 0x100 | 2 },
 { (3 << 16) | VDP2::GSREG_HRES, "HRES", "Horizontal Resolution", 0x100 | 3 },

 { 0, "------", "", 0xFFFF },

 { (3 << 16) | VDP2::GSREG_RAMCTL, "RAMCTL", "RAM Control", 2 },
 { (3 << 16) | VDP2::GSREG_CYCA0, "A0", "CYCA0", 4 },
 { (3 << 16) | VDP2::GSREG_CYCA1, "A1", "CYCA1", 4 },
 { (3 << 16) | VDP2::GSREG_CYCB0, "B0", "CYCB0", 4 },
 { (3 << 16) | VDP2::GSREG_CYCB1, "B1", "CYCB1", 4 },

 { 0, "------", "", 0xFFFF },

 { (3 << 16) | VDP2::GSREG_BGON, "BGON", "BGON", 2 },
 { (3 << 16) | VDP2::GSREG_MZCTL, "MZCTL", "MZCTL", 2 },
 { (3 << 16) | VDP2::GSREG_SFSEL, "SFSEL", "SFSEL", 2 },
 { (3 << 16) | VDP2::GSREG_SFCODE, "SFCODE", "SFCODE", 2 },
 { (3 << 16) | VDP2::GSREG_CHCTLA, "CHCTLA", "CHCTLA", 2 },
 { (3 << 16) | VDP2::GSREG_CHCTLB, "CHCTLB", "CHCTLB", 2 },

 { 0, "------", "", 0xFFFF },

 { (3 << 16) | VDP2::GSREG_SCXIN0, "SCXIN0", "SCXIN0", 2 },
 { (3 << 16) | VDP2::GSREG_SCXDN0, "SCXDN0", "SCXDN0", 2 },
 { (3 << 16) | VDP2::GSREG_SCYIN0, "SCYIN0", "SCYIN0", 2 },
 { (3 << 16) | VDP2::GSREG_SCYDN0, "SCYDN0", "SCYDN0", 2 },
 { (3 << 16) | VDP2::GSREG_ZMXIN0, "ZMXIN0", "ZMXIN0", 2 },
 { (3 << 16) | VDP2::GSREG_ZMXDN0, "ZMXDN0", "ZMXDN0", 2 },
 { (3 << 16) | VDP2::GSREG_ZMYIN0, "ZMYIN0", "ZMYIN0", 2 },
 { (3 << 16) | VDP2::GSREG_ZMYDN0, "ZMYDN0", "ZMYDN0", 2 },

 { (3 << 16) | VDP2::GSREG_SCXIN1, "SCXIN1", "SCXIN1", 2 },
 { (3 << 16) | VDP2::GSREG_SCXDN1, "SCXDN1", "SCXDN1", 2 },
 { (3 << 16) | VDP2::GSREG_SCYIN1, "SCYIN1", "SCYIN1", 2 },
 { (3 << 16) | VDP2::GSREG_SCYDN1, "SCYDN1", "SCYDN1", 2 },
 { (3 << 16) | VDP2::GSREG_ZMXIN1, "ZMXIN1", "ZMXIN1", 2 },
 { (3 << 16) | VDP2::GSREG_ZMXDN1, "ZMXDN1", "ZMXDN1", 2 },
 { (3 << 16) | VDP2::GSREG_ZMYIN1, "ZMYIN1", "ZMYIN1", 2 },
 { (3 << 16) | VDP2::GSREG_ZMYDN1, "ZMYDN1", "ZMYDN1", 2 },

 { (3 << 16) | VDP2::GSREG_SCYN3, "SCXN2", "SCXN2", 2 },
 { (3 << 16) | VDP2::GSREG_SCYN3, "SCYN2", "SCYN2", 2 },
 { (3 << 16) | VDP2::GSREG_SCYN3, "SCXN3", "SCXN3", 2 },
 { (3 << 16) | VDP2::GSREG_SCYN3, "SCYN3", "SCYN3", 2 },

 { (3 << 16) | VDP2::GSREG_ZMCTL, "ZMCTL", "ZMCTL", 2 },
 { (3 << 16) | VDP2::GSREG_SCRCTL, "SCRCTL", "SCRCTL", 2 },

 { 0, "------", "", 0xFFFF },
 { (3 << 16) | VDP2::GSREG_RPMD, "RPMD", "RPMD", 1 },
 { (3 << 16) | VDP2::GSREG_RPRCTL, "RPRCTL", "RPRCTL", 2 },
 { (3 << 16) | VDP2::GSREG_KTCTL, "KTCTL", "KTCTL", 2 },
 { (3 << 16) | VDP2::GSREG_KTAOF, "KTAOF", "KTAOF", 2 },
 { (3 << 16) | VDP2::GSREG_OVPNRA, "OVPNRA", "OVPNRA", 2 },
 { (3 << 16) | VDP2::GSREG_OVPNRB, "OVPNRB", "OVPNRB", 2 },
 { (3 << 16) | VDP2::GSREG_RPTA, "RPTA", "RPTA", 0x100 | 19 },


 { 0, "------", "", 0xFFFF },
 { (3 << 16) | VDP2::GSREG_PRINA, "PRINA", "PRINA", 2 },
 { (3 << 16) | VDP2::GSREG_PRINB, "PRINB", "PRINB", 2 },
 { (3 << 16) | VDP2::GSREG_PRIR, "PRIR", "PRIR", 2 },


 { 0, "", "", 0 },
};

static const RegGroupType DBG_RegGroup_VDP2 =
{
 NULL,
 DBG_Regs_VDP2,
 GetRegister,
 SetRegister
};

//
//
//
static MDFN_COLD void DBG_Init(void)
{
 DBG.ActiveCPU = 0;
 DBG.BTEnabled = false;

 for(unsigned which = 0; which < 2; which++)
 {
  DBG.BTIndex[which] = 0;

  for(unsigned i = 0; i < DBGS::NUMBT; i++)
   DBG.BTEntries[which][i].valid = false;
 }

 DBG.CPUHook = NULL;
 DBG.CPUHookContinuous = false;
 DBG.FoundBPoint = false;

 DBG.eff_ts = 0;
 DBG.inss = false;

 MDFNDBG_AddRegGroup(&DBG_RegGroup_CPU);
 MDFNDBG_AddRegGroup(&DBG_RegGroup_CPUE_SCSP);
 MDFNDBG_AddRegGroup(&DBG_RegGroup_SCU_CDB_VDP1);
 MDFNDBG_AddRegGroup(&DBG_RegGroup_VDP2);
 //
 //
 //
 ASpace_Add(GetAddressSpaceBytes<ASPACE_PHYSICAL>, PutAddressSpaceBytes<ASPACE_PHYSICAL>, "physical", "SH-2 External Bus (TODO)", 27);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_WORKRAML>, PutAddressSpaceBytes<ASPACE_WORKRAML>, "workraml", "Low Work RAM", 20);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_WORKRAMH>, PutAddressSpaceBytes<ASPACE_WORKRAMH>, "workramh", "High Work RAM", 20);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_SCSPRAM>, PutAddressSpaceBytes<ASPACE_SCSPRAM>, "scspram", "SCSP RAM", 19);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_SCSPMPROG>, PutAddressSpaceBytes<ASPACE_SCSPMPROG>, "scspmprog", "SCSP MPROG", 10);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_SCSPTEMP>, PutAddressSpaceBytes<ASPACE_SCSPTEMP>, "scsptemp", "SCSP TEMP, Relative Offset", 9);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_SCSPMEMS>, PutAddressSpaceBytes<ASPACE_SCSPMEMS>, "scspmems", "SCSP MEMS", 7);

 ASpace_Add(GetAddressSpaceBytes<ASPACE_DSPPROG>, PutAddressSpaceBytes<ASPACE_DSPPROG>, "dspprog", "DSP Program RAM", 10);

 ASpace_Add(GetAddressSpaceBytes<ASPACE_BACKUP>, PutAddressSpaceBytes<ASPACE_BACKUP>, "backup", "Backup RAM", 15);

 ASpace_Add(GetAddressSpaceBytes<ASPACE_VDP1VRAM>, PutAddressSpaceBytes<ASPACE_VDP1VRAM>, "vdp1vram", "VDP1 VRAM", 19);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_VDP1FB0>, PutAddressSpaceBytes<ASPACE_VDP1FB0>, "vdp1fb0",  "VDP1 FB0 RAM(raw, no 8bpp/rot addr bit swizzling)", 18);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_VDP1FB1>, PutAddressSpaceBytes<ASPACE_VDP1FB1>, "vdp1fb1",  "VDP1 FB1 RAM(raw, no 8bpp/rot addr bit swizzling)", 18);
 ASpace_Add(GetAddressSpaceBytes<ASPACE_VDP2VRAM>, PutAddressSpaceBytes<ASPACE_VDP2VRAM>, "vdp2vram", "VDP2 VRAM", 19);
}

static MDFN_COLD void DBG_Kill(void)
{
 DBG.BreakPointsPC.clear();
 DBG.BreakPointsRead.clear();
 DBG.BreakPointsWrite.clear();
}

static DebuggerInfoStruct DBGInfo
{
 false,
 "SJIS",

 2,
 2,

 32,
 27,

 0x06004000,
 ~0U,

 DBG_MemPeek,

 DBG_Disassemble,
 DBG_ToggleSyntax,

 NULL,
 NULL,

 DBG_FlushBreakPoints,
 DBG_AddBreakPoint,
 DBG_SetCPUCallback,

 DBG_EnableBranchTrace,
 DBG_GetBranchTrace,

 NULL,
 NULL,

 NULL,
 NULL,
};
#else
static INLINE void DBG_SetEffTS(const sscpu_timestamp_t eff_ts) { }
static INLINE bool DBG_InSlaveStep(void) { return false; }
template<unsigned which> static INLINE void DBG_CPUHandler(void) { }
static INLINE void DBG_AddBranchTrace(unsigned which, uint32 to, int exception, unsigned vecnum = 0) { }
static INLINE bool DBG_NeedCPUHooks(void) { return false; } 
static INLINE void DBG_Init(void) { }
static INLINE void DBG_Kill(void) { }
#endif
