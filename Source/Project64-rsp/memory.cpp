enum
{
    MaxMaps = 32
};

#include "RSP Registers.h"
#include "Rsp.h"
#include <windows.h>

DWORD NoOfMaps, MapsCRC[MaxMaps];
uint32_t Table;
BYTE *RecompCode, *RecompCodeSecondary, *RecompPos, *JumpTables;
void ** JumpTable;

int AllocateMemory(void)
{
    if (RecompCode == NULL)
    {
        RecompCode = (BYTE *)VirtualAlloc(NULL, 0x00400004, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        RecompCode = (BYTE *)VirtualAlloc(RecompCode, 0x00400000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

        if (RecompCode == NULL)
        {
            DisplayError("Not enough memory for RSP RecompCode!");
            return false;
        }
    }

    if (RecompCodeSecondary == NULL)
    {
        RecompCodeSecondary = (BYTE *)VirtualAlloc(NULL, 0x00200000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (RecompCodeSecondary == NULL)
        {
            DisplayError("Not enough memory for RSP RecompCode Secondary!");
            return false;
        }
    }

    if (JumpTables == NULL)
    {
        JumpTables = (BYTE *)VirtualAlloc(NULL, 0x1000 * MaxMaps, MEM_COMMIT, PAGE_READWRITE);
        if (JumpTables == NULL)
        {
            DisplayError("Not enough memory for jump table!");
            return false;
        }
    }

    JumpTable = (void **)JumpTables;
    RecompPos = RecompCode;
    NoOfMaps = 0;
    return true;
}

void FreeMemory(void)
{
    VirtualFree(RecompCode, 0, MEM_RELEASE);
    VirtualFree(JumpTable, 0, MEM_RELEASE);
    VirtualFree(RecompCodeSecondary, 0, MEM_RELEASE);

    RecompCode = NULL;
    JumpTables = NULL;
    RecompCodeSecondary = NULL;
}

void ResetJumpTables(void)
{
    memset(JumpTables, 0, 0x1000 * MaxMaps);
    RecompPos = RecompCode;
    NoOfMaps = 0;
}

void SetJumpTable(uint32_t End)
{
    DWORD CRC, count;

    CRC = 0;
    if (End < 0x800)
    {
        End = 0x800;
    }

    if (End == 0x1000 && ((*RSPInfo.SP_MEM_ADDR_REG & 0x0FFF) & ~7) == 0x80)
    {
        End = 0x800;
    }

    for (count = 0; count < End; count += 0x40)
    {
        CRC += *(DWORD *)(RSPInfo.IMEM + count);
    }

    for (count = 0; count < NoOfMaps; count++)
    {
        if (CRC == MapsCRC[count])
        {
            JumpTable = (void **)(JumpTables + count * 0x1000);
            Table = count;
            return;
        }
    }
    //DisplayError("%X %X",NoOfMaps,CRC);
    if (NoOfMaps == MaxMaps)
    {
        ResetJumpTables();
    }
    MapsCRC[NoOfMaps] = CRC;
    JumpTable = (void **)(JumpTables + NoOfMaps * 0x1000);
    Table = NoOfMaps;
    NoOfMaps += 1;
}

void RSP_LB_DMEM(uint32_t Addr, uint8_t * Value)
{
    *Value = *(uint8_t *)(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF));
}

void RSP_LBV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    RSP_Vect[vect].s8(15 - element) = *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF));
}

void RSP_LDV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    uint8_t length, Count;

    length = 8;
    if (length > 16 - element)
    {
        length = 16 - element;
    }
    for (Count = element; Count < (length + element); Count++)
    {
        RSP_Vect[vect].s8(15 - Count) = *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF));
        Addr += 1;
    }
}

void RSP_LFV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    uint8_t length, count;
    RSPVector Temp;

    length = 8;
    if (length > 16 - element)
    {
        length = 16 - element;
    }

    Temp.s16(7) = *(RSPInfo.DMEM + (((Addr + element) ^ 3) & 0xFFF)) << 7;
    Temp.s16(6) = *(RSPInfo.DMEM + (((Addr + ((0x4 - element) ^ 3) & 0xf)) & 0xFFF)) << 7;
    Temp.s16(5) = *(RSPInfo.DMEM + (((Addr + ((0x8 - element) ^ 3) & 0xf)) & 0xFFF)) << 7;
    Temp.s16(4) = *(RSPInfo.DMEM + (((Addr + ((0xC - element) ^ 3) & 0xf)) & 0xFFF)) << 7;
    Temp.s16(3) = *(RSPInfo.DMEM + (((Addr + ((0x8 - element) ^ 3) & 0xf)) & 0xFFF)) << 7;
    Temp.s16(2) = *(RSPInfo.DMEM + (((Addr + ((0xC - element) ^ 3) & 0xf)) & 0xFFF)) << 7;
    Temp.s16(1) = *(RSPInfo.DMEM + (((Addr + ((0x10 - element) ^ 3) & 0xf)) & 0xFFF)) << 7;
    Temp.s16(0) = *(RSPInfo.DMEM + (((Addr + ((0x4 - element) ^ 3) & 0xf)) & 0xFFF)) << 7;

    for (count = element; count < (length + element); count++)
    {
        RSP_Vect[vect].s8(15 - count) = Temp.s8(15 - count);
    }
}

void RSP_LH_DMEM(uint32_t Addr, uint16_t * Value)
{
    if ((Addr & 0x1) != 0)
    {
        *Value = *(uint8_t *)(RSPInfo.DMEM + (((Addr + 0) & 0xFFF) ^ 3)) << 8;
        *Value += *(uint8_t *)(RSPInfo.DMEM + (((Addr + 1) & 0xFFF) ^ 3)) << 0;
        return;
    }
    *Value = *(uint16_t *)(RSPInfo.DMEM + ((Addr ^ 2) & 0xFFF));
}

void RSP_LHV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    RSP_Vect[vect].s16(7) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(6) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 2) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(5) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 4) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(4) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 6) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(3) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 8) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(2) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 10) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(1) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 12) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(0) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 14) & 0xF) ^ 3) & 0xFFF)) << 7;
}

void RSP_LLV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    uint8_t length, Count;

    length = 4;
    if (length > 16 - element)
    {
        length = 16 - element;
    }
    for (Count = element; Count < (length + element); Count++)
    {
        RSP_Vect[vect].s8(15 - Count) = *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF));
        Addr += 1;
    }
}

void RSP_LPV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    RSP_Vect[vect].s16(7) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element) & 0xF) ^ 3) & 0xFFF)) << 8;
    RSP_Vect[vect].s16(6) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 1) & 0xF) ^ 3) & 0xFFF)) << 8;
    RSP_Vect[vect].s16(5) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 2) & 0xF) ^ 3) & 0xFFF)) << 8;
    RSP_Vect[vect].s16(4) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 3) & 0xF) ^ 3) & 0xFFF)) << 8;
    RSP_Vect[vect].s16(3) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 4) & 0xF) ^ 3) & 0xFFF)) << 8;
    RSP_Vect[vect].s16(2) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 5) & 0xF) ^ 3) & 0xFFF)) << 8;
    RSP_Vect[vect].s16(1) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 6) & 0xF) ^ 3) & 0xFFF)) << 8;
    RSP_Vect[vect].s16(0) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 7) & 0xF) ^ 3) & 0xFFF)) << 8;
}

void RSP_LRV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    uint8_t length, Count, offset;

    offset = (Addr & 0xF) - 1;
    length = (Addr & 0xF) - element;
    Addr &= 0xFF0;
    for (Count = element; Count < (length + element); Count++)
    {
        RSP_Vect[vect].s8(offset - Count) = *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF));
        Addr += 1;
    }
}

void RSP_LQV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    uint32_t length = ((Addr + 0x10) & ~0xF) - Addr;
    if (length > 16 - element)
    {
        length = 16 - element;
    }
    for (uint8_t Count = element; Count < (length + element); Count++)
    {
        RSP_Vect[vect].s8(15 - Count) = *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF));
        Addr += 1;
    }
}

void RSP_LSV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    uint8_t length, Count;

    length = 2;
    if (length > 16 - element)
    {
        length = 16 - element;
    }
    for (Count = element; Count < (length + element); Count++)
    {
        RSP_Vect[vect].s8(15 - Count) = *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF));
        Addr += 1;
    }
}

void RSP_LTV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    uint8_t del, count, length;

    length = 8;
    if (length > 32 - vect)
    {
        length = 32 - vect;
    }

    Addr = ((Addr + 8) & 0xFF0) + (element & 0x1);
    for (count = 0; count < length; count++)
    {
        del = ((8 - (element >> 1) + count) << 1) & 0xF;
        RSP_Vect[vect + count].s8(15 - del) = *(RSPInfo.DMEM + (Addr ^ 3));
        RSP_Vect[vect + count].s8(14 - del) = *(RSPInfo.DMEM + ((Addr + 1) ^ 3));
        Addr += 2;
    }
}

void RSP_LUV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    RSP_Vect[vect].s16(7) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(6) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 1) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(5) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 2) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(4) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 3) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(3) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 4) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(2) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 5) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(1) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 6) & 0xF) ^ 3) & 0xFFF)) << 7;
    RSP_Vect[vect].s16(0) = *(RSPInfo.DMEM + ((Addr + ((0x10 - element + 7) & 0xF) ^ 3) & 0xFFF)) << 7;
}

void RSP_LW_DMEM(uint32_t Addr, uint32_t * Value)
{
    if ((Addr & 0x3) != 0)
    {
        *Value = *(uint8_t *)(RSPInfo.DMEM + (((Addr + 0) & 0xFFF) ^ 3)) << 24;
        *Value += *(uint8_t *)(RSPInfo.DMEM + (((Addr + 1) & 0xFFF) ^ 3)) << 16;
        *Value += *(uint8_t *)(RSPInfo.DMEM + (((Addr + 2) & 0xFFF) ^ 3)) << 8;
        *Value += *(uint8_t *)(RSPInfo.DMEM + (((Addr + 3) & 0xFFF) ^ 3)) << 0;
        return;
    }
    *Value = *(uint32_t *)(RSPInfo.DMEM + (Addr & 0xFFF));
}

void RSP_LW_IMEM(uint32_t Addr, uint32_t * Value)
{
    if ((Addr & 0x3) != 0)
    {
        DisplayError("Unaligned RSP_LW_IMEM");
    }
    *Value = *(uint32_t *)(RSPInfo.IMEM + (Addr & 0xFFF));
}

void RSP_SB_DMEM(uint32_t Addr, uint8_t Value)
{
    *(uint8_t *)(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = Value;
}

void RSP_SBV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect].s8(15 - element);
}

void RSP_SDV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    int Count;

    for (Count = element; Count < (8 + element); Count++)
    {
        *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect].s8(15 - (Count & 0xF));
        Addr += 1;
    }
}

void RSP_SFV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    int offset = Addr & 0xF;
    Addr &= 0xFF0;

    switch (element)
    {
    case 0:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = (RSP_Vect[vect].u16(7) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(6) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(5) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(4) >> 7) & 0xFF;
        break;
    case 1:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = (RSP_Vect[vect].u16(1) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(0) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(3) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(2) >> 7) & 0xFF;
        break;
    case 2:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = 0;
        break;
    case 3:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF) ^ 3))) = 0;
        break;
    case 4:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = (RSP_Vect[vect].u16(6) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(5) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(4) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(7) >> 7) & 0xFF;
        break;
    case 5:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = (RSP_Vect[vect].u16(0) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(3) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(2) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(1) >> 7) & 0xFF;
        break;
    case 6:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = 0;
        break;
    case 7:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = 0;
        break;
    case 8:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = (RSP_Vect[vect].u16(3) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(2) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(1) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(0) >> 7) & 0xFF;
        break;
    case 9:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = 0;
        break;
    case 10:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = 0;
        break;
    case 11:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = (RSP_Vect[vect].u16(4) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(7) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(6) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(5) >> 7) & 0xFF;
        break;
    case 12:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = (RSP_Vect[vect].u16(2) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(1) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(0) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(3) >> 7) & 0xFF;
        break;
    case 13:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = 0;
        break;
    case 14:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = 0;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = 0;
        break;
    case 15:
        *(RSPInfo.DMEM + ((Addr + offset) ^ 3)) = (RSP_Vect[vect].u16(7) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 4) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(6) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 8) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(5) >> 7) & 0xFF;
        *(RSPInfo.DMEM + ((Addr + ((offset + 12) & 0xF)) ^ 3)) = (RSP_Vect[vect].u16(4) >> 7) & 0xFF;
        break;
    }
}

void RSP_SH_DMEM(uint32_t Addr, uint16_t Value)
{
    if ((Addr & 0x1) != 0)
    {
        *(uint8_t *)(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = (Value >> 8);
        *(uint8_t *)(RSPInfo.DMEM + (((Addr + 1) ^ 3) & 0xFFF)) = (Value & 0xFF);
    }
    else
    {
        *(uint16_t *)(RSPInfo.DMEM + ((Addr ^ 2) & 0xFFF)) = Value;
    }
}

void RSP_SHV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8((15 - element) & 0xF) << 1) +
                                             (RSP_Vect[vect].u8((14 - element) & 0xF) >> 7);
    *(RSPInfo.DMEM + (((Addr + 2) ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8((13 - element) & 0xF) << 1) +
                                                   (RSP_Vect[vect].u8((12 - element) & 0xF) >> 7);
    *(RSPInfo.DMEM + (((Addr + 4) ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8((11 - element) & 0xF) << 1) +
                                                   (RSP_Vect[vect].u8((10 - element) & 0xF) >> 7);
    *(RSPInfo.DMEM + (((Addr + 6) ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8((9 - element) & 0xF) << 1) +
                                                   (RSP_Vect[vect].u8((8 - element) & 0xF) >> 7);
    *(RSPInfo.DMEM + (((Addr + 8) ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8((7 - element) & 0xF) << 1) +
                                                   (RSP_Vect[vect].u8((6 - element) & 0xF) >> 7);
    *(RSPInfo.DMEM + (((Addr + 10) ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8((5 - element) & 0xF) << 1) +
                                                    (RSP_Vect[vect].u8((4 - element) & 0xF) >> 7);
    *(RSPInfo.DMEM + (((Addr + 12) ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8((3 - element) & 0xF) << 1) +
                                                    (RSP_Vect[vect].u8((2 - element) & 0xF) >> 7);
    *(RSPInfo.DMEM + (((Addr + 14) ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8((1 - element) & 0xF) << 1) +
                                                    (RSP_Vect[vect].u8((0 - element) & 0xF) >> 7);
}

void RSP_SLV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    int Count;

    for (Count = element; Count < (4 + element); Count++)
    {
        *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect].s8(15 - (Count & 0xF));
        Addr += 1;
    }
}

void RSP_SPV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    int Count;

    for (Count = element; Count < (8 + element); Count++)
    {
        if (((Count)&0xF) < 8)
        {
            *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect].u8(15 - ((Count & 0xF) << 1));
        }
        else
        {
            *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = (RSP_Vect[vect].u8(15 - ((Count & 0x7) << 1)) << 1) +
                                                     (RSP_Vect[vect].u8(14 - ((Count & 0x7) << 1)) >> 7);
        }
        Addr += 1;
    }
}

void RSP_SQV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    int length, Count;

    length = ((Addr + 0x10) & ~0xF) - Addr;
    for (Count = element; Count < (length + element); Count++)
    {
        *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect].s8(15 - (Count & 0xF));
        Addr += 1;
    }
}

void RSP_SRV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    int length, Count, offset;

    length = (Addr & 0xF);
    offset = (0x10 - length) & 0xF;
    Addr &= 0xFF0;
    for (Count = element; Count < (length + element); Count++)
    {
        *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect].s8(15 - ((Count + offset) & 0xF));
        Addr += 1;
    }
}

void RSP_SSV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    int Count;

    for (Count = element; Count < (2 + element); Count++)
    {
        *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect].s8(15 - (Count & 0xF));
        Addr += 1;
    }
}

void RSP_STV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    uint8_t del, count, length;

    length = 8;
    if (length > 32 - vect)
    {
        length = 32 - vect;
    }
    length = length << 1;
    del = element >> 1;
    for (count = 0; count < length; count += 2)
    {
        *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect + del].u8(15 - count);
        *(RSPInfo.DMEM + (((Addr + 1) ^ 3) & 0xFFF)) = RSP_Vect[vect + del].u8(14 - count);
        del = (del + 1) & 7;
        Addr += 2;
    }
}

void RSP_SUV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    for (uint8_t Count = element; Count < (8 + element); Count++)
    {
        if (((Count)&0xF) < 8)
        {
            *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = ((RSP_Vect[vect].u8(15 - ((Count & 0x7) << 1)) << 1) +
                                                      (RSP_Vect[vect].u8(14 - ((Count & 0x7) << 1)) >> 7)) &
                                                     0xFF;
        }
        else
        {
            *(RSPInfo.DMEM + ((Addr ^ 3) & 0xFFF)) = RSP_Vect[vect].u8(15 - ((Count & 0x7) << 1));
        }
        Addr += 1;
    }
}

void RSP_SW_DMEM(uint32_t Addr, uint32_t Value)
{
    if ((Addr & 0x3) != 0)
    {
        *(uint8_t *)(RSPInfo.DMEM + (((Addr + 0) ^ 3) & 0xFFF)) = (Value >> 24) & 0xFF;
        *(uint8_t *)(RSPInfo.DMEM + (((Addr + 1) ^ 3) & 0xFFF)) = (Value >> 16) & 0xFF;
        *(uint8_t *)(RSPInfo.DMEM + (((Addr + 2) ^ 3) & 0xFFF)) = (Value >> 8) & 0xFF;
        *(uint8_t *)(RSPInfo.DMEM + (((Addr + 3) ^ 3) & 0xFFF)) = (Value >> 0) & 0xFF;
    }
    else
    {
        *(uint32_t *)(RSPInfo.DMEM + (Addr & 0xFFF)) = Value;
    }
}

void RSP_SWV_DMEM(uint32_t Addr, uint8_t vect, uint8_t element)
{
    int Count, offset;

    offset = Addr & 0xF;
    Addr &= 0xFF0;
    for (Count = element; Count < (16 + element); Count++)
    {
        *(RSPInfo.DMEM + ((Addr + (offset & 0xF)) ^ 3)) = RSP_Vect[vect].s8(15 - (Count & 0xF));
        offset += 1;
    }
}
