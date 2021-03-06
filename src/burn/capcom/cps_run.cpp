// CPS - Run
#include "cps.h"

// Inputs:
unsigned char CpsReset = 0;
unsigned char Cpi01A = 0, Cpi01C = 0, Cpi01E = 0;

static int nInterrupt;
static int nIrqLine, nIrqCycles;
static bool bEnableAutoIrq50, bEnableAutoIrq52;			// Trigger an interrupt every 32 scanlines

static const int nFirstLine = 0x0C;				// The first scanline of the display
static const int nVBlank = 0x0106 - 0x0C;			// The scanline at which the vblank interrupt is triggered

static int nCpsCyclesExtra;

int nIrqLine50, nIrqLine52;

static int DrvReset()
{
	extern unsigned char* CpsMem;
	if (CpsMem)
		memset (CpsMem, 0, 0x40000); // Clear GFX and main RAM

	// Reset machine
	if (Cps == 2 || (kludge == 5) || Cps1Qs == 1)
		EEPROMReset();

	SekOpen(0);
	SekReset();
	SekClose();

	if (!Cps1Pic) {
		ZetOpen(0);
		ZetReset();
		ZetClose();
	}

	if (Cps == 2) {
		// Disable beam-synchronized interrupts
#ifdef LSB_FIRST
		*((unsigned short*)(CpsReg + 0x4E)) = 0x0200;
		*((unsigned short*)(CpsReg + 0x50)) = 0x0106;
		*((unsigned short*)(CpsReg + 0x52)) = 0x0106;
#else
		*((unsigned short*)(CpsReg + 0x4E)) = 0x0002;
		*((unsigned short*)(CpsReg + 0x50)) = 0x0601;
		*((unsigned short*)(CpsReg + 0x52)) = 0x0601;
#endif
	}

	CpsMapObjectBanks(0);

	nCpsCyclesExtra = 0;

	if (Cps == 2 || Cps1Qs == 1)
		QsndReset(); // Sound init (QSound)

	BurnAfterReset();

	return 0;
}

static const eeprom_interface qsound_eeprom_interface =
{
	7,		/* address bits */
	8,		/* data bits */
	"0110",	/*  read command */
	"0101",	/* write command */
	"0111",	/* erase command */
	0,
	0,
	0,
	0
};

static const eeprom_interface cps2_eeprom_interface =
{
	6,		/* address bits */
	16,		/* data bits */
	"0110",	/*  read command */
	"0101",	/* write command */
	"0111",	/* erase command */
	0,
	0,
	0,
	0
};

int Cps2RunInit()
{
	nLagObjectPalettes = 0;

	nLagObjectPalettes = 1;

	SekInit(0, 0x68000);					// Allocate 68000

	if (Cps2MemInit())					// Memory init
		return 1;

	EEPROMInit(&cps2_eeprom_interface);

	CpsRwInit();							// Registers setup

	if (CpsPalInit())						// Palette init
		return 1;

	if (CpsObjInit())						// Sprite init
		return 1;

	// Sound init (QSound)
	if (QsndInit())
		return 1;

	EEPROMReset();

	DrvReset();

	return 0;
}

int CpsRunInit()
{
	nLagObjectPalettes = 0;

	if (Cps == 2) nLagObjectPalettes = 1;

	SekInit(0, 0x68000);					// Allocate 68000

	if (CpsMemInit()) {						// Memory init
		return 1;
	}

	if (Cps == 2 || (kludge == 5))
		EEPROMInit(&cps2_eeprom_interface);
	else
	{
		if (Cps1Qs == 1)
			EEPROMInit(&qsound_eeprom_interface);
	}

	CpsRwInit();							// Registers setup

	if (CpsPalInit())						// Palette init
		return 1;

	if (CpsObjInit())						// Sprite init
		return 1;

	if ((Cps & 1) && Cps1Qs == 0 && Cps1Pic == 0) {			// Sound init (MSM6295 + YM2151)
		if (PsndInit())
			return 1;
	}

	if (Cps == 2 || Cps1Qs == 1) {			// Sound init (QSound)
		if (QsndInit())
			return 1;
	}

	if (Cps == 2 || (kludge == 5) || Cps1Qs == 1)
		EEPROMReset();

	DrvReset();

	return 0;
}

int CpsRunExit()
{
	if (Cps == 2 || (kludge == 5) || Cps1Qs == 1) EEPROMExit();

	// Sound exit
	if (Cps == 2 || Cps1Qs == 1)
		QsndExit();
	if (Cps != 2 && Cps1Qs == 0)
		PsndExit();

	// Graphics exit
	CpsObjExit();
	CpsPalExit();

	// Sprite Masking exit
	ZBuf = NULL;

	// Memory exit
	CpsRwExit();
	CpsMemExit();

	SekExit();

	return 0;
}

// nStart = 0-3, nCount=1-4
static inline void GetPalette(int nStart, int nCount)
{
	// Update Palette (Ghouls points to the wrong place on boot up I think)
	unsigned short val = (*(unsigned short*)(CpsReg + 0x0A));
	int nPal = (swapWord(val) << 8) & 0xFFF800;

	unsigned char* Find = CpsFindGfxRam(nPal, 0x1000);
	if (Find)
		memcpy(CpsSavePal + (nStart << 10), Find + (nStart << 10), nCount << 10);
}

static void GetStarPalette()
{
	int nPal = (*((unsigned short*)(CpsReg + 0x0A)) << 8) & 0xFFF800;

	unsigned char* Find = CpsFindGfxRam(nPal, 256);
	if (Find) {
		memcpy(CpsSavePal + 4096, Find + 4096, 256);
		memcpy(CpsSavePal + 5120, Find + 5120, 256);
	}
}

#define CopyCpsReg(i) \
	memcpy(CpsSaveReg[i], CpsReg, 0x0100);

#define CopyCpsFrg(i) \
	memcpy(CpsSaveFrg[i], CpsFrg, 0x0010);

// Schedule a beam-synchronized interrupt
#define ScheduleIRQ() \
	int nLine = 0x0106; \
	if (nIrqLine50 <= nLine) { \
		nLine = nIrqLine50; \
	} \
	if (nIrqLine52 < nLine) { \
		nLine = nIrqLine52; \
	} \
	if (nLine < 0x0106) { \
		nIrqLine = nLine; \
		nIrqCycles = (nLine * nCpsCycles / 0x0106) + 1; \
	} else { \
		nIrqCycles = nCpsCycles + 1; \
	}

// Execute a beam-synchronised interrupt and schedule the next one
static void DoIRQ()
{
	// 0x4E - bit 9 = 1: Beam Synchronized interrupts disabled
	// 0x50 - Beam synchronized interrupt #1 occurs at raster line.
	// 0x52 - Beam synchronized interrupt #2 occurs at raster line.

	// Trigger IRQ and copy registers.
	if (nIrqLine >= nFirstLine) {
		nInterrupt++;
		nRasterline[nInterrupt] = nIrqLine - nFirstLine;
	}

	SekSetIRQLine(4, SEK_IRQSTATUS_AUTO);
	SekRun(nCpsCycles * 0x01 / 0x0106);
	if (nRasterline[nInterrupt] < 224)
	{
		CopyCpsReg(nInterrupt);
		CopyCpsFrg(nInterrupt);
	}
	else
		nRasterline[nInterrupt] = 0;

	// Schedule next interrupt
	if (!bEnableAutoIrq50) {
		if (nIrqLine >= nIrqLine50)
			nIrqLine50 = 0x0106;
	}
	else
	{
		if (bEnableAutoIrq50 && nIrqLine == nIrqLine50)
			nIrqLine50 += 32;
	}
	if (!bEnableAutoIrq52 && nIrqLine >= nIrqLine52)
		nIrqLine52 = 0x0106;
	else
	{
		if (bEnableAutoIrq52 && nIrqLine == nIrqLine52)
			nIrqLine52 += 32;
	}
	ScheduleIRQ();
	if (nIrqCycles < SekTotalCycles())
		nIrqCycles = SekTotalCycles() + 1;

	return;
}

int Cps1Frame()
{
	int nDisplayEnd, nNext, i;

	if (CpsReset)
		DrvReset();

	SekNewFrame();
	if (Cps1Qs == 1)
		QsndNewFrame();
	else
	{
		if (!Cps1Pic) {
			ZetOpen(0);
			PsndNewFrame();
		}
	}

	nCpsCycles = (int)((long long)nCPS68KClockspeed * nBurnCPUSpeedAdjust >> 8);

	Cps1RwGetInp();							// Update the input port values

	nDisplayEnd = (nCpsCycles * (nFirstLine + 224)) / 0x0106;	// Account for VBlank

	SekOpen(0);
	SekIdle(nCpsCyclesExtra);

	SekRun(nCpsCycles * nFirstLine / 0x0106);			// run 68K for the first few lines

	if (kludge != 10 && kludge != 21)
		CpsObjGet();						// Get objects

	for (i = 0; i < 4; i++) {
		nNext = ((i + 1) * nCpsCycles) >> 2;			// find out next cycle count to run to

		if (SekTotalCycles() < nDisplayEnd && nNext > nDisplayEnd) {

			SekRun(nNext - nDisplayEnd);			// run 68K

			memcpy(CpsSaveReg[0], CpsReg, 0x100);		// Registers correct now

			GetPalette(0, 6);				// Get palette
			if (CpsStar) {
				GetStarPalette();
			}

			if (kludge == 10 || kludge == 21) {
				if (i == 3)
					CpsObjGet();   		// Get objects
			}

			SekSetIRQLine(2, SEK_IRQSTATUS_AUTO);		// Trigger VBlank interrupt
		}

		SekRun(nNext - SekTotalCycles());			// run 68K
	}

	if (pBurnDraw)
		CpsDraw();						// Draw frame

	if (Cps1Qs == 1)
		QsndEndFrame();
	else
	{
		if (!Cps1Pic) {
			PsndSyncZ80(nCpsZ80Cycles);
			PsmUpdate(nBurnSoundLen);
			ZetClose();
		}
	}

	nCpsCyclesExtra = SekTotalCycles() - nCpsCycles;

	SekClose();

	return 0;
}

int Cps2Frame()
{
	int nDisplayEnd, nNext;									// variables to keep track of executed 68K cyles
	int i;

	if (CpsReset)
		DrvReset();

	//	extern int prevline;
	//	prevline = -1;

	SekNewFrame();
	QsndNewFrame();

	nCpsCycles = (int)(((long long)nCPS68KClockspeed * nBurnCPUSpeedAdjust) >> 8);
	SekSetCyclesScanline(nCpsCycles / 262);

	Cps2RwGetInp();	// Update the input port values

	nDisplayEnd = nCpsCycles * (nFirstLine + 224) / 0x0106;	// Account for VBlank

	nInterrupt = 0;

	for (i = 0; i < MAX_RASTER + 2; i++)
		nRasterline[i] = 0;

	// Determine which (if any) of the line counters generates the first IRQ
	bEnableAutoIrq50 = bEnableAutoIrq52 = false;
	nIrqLine50 = nIrqLine52 = 0x0106;

	if (swapWord(*((unsigned short*)(CpsReg + 0x50))) & 0x8000)
		bEnableAutoIrq50 = true;

	if (bEnableAutoIrq50 || (swapWord(*((unsigned short*)(CpsReg + 0x4E))) & 0x0200) == 0)
		nIrqLine50 = swapWord((*((unsigned short*)(CpsReg + 0x50))) & 0x01FF);

	if (swapWord(*((unsigned short*)(CpsReg + 0x52))) & 0x8000)
		bEnableAutoIrq52 = true;

	if (bEnableAutoIrq52 || (swapWord(*((unsigned short*)(CpsReg + 0x4E))) & 0x0200) == 0)
		nIrqLine52 = (swapWord(*((unsigned short*)(CpsReg + 0x52))) & 0x01FF);

	ScheduleIRQ();

	SekOpen(0);
	SekIdle(nCpsCyclesExtra);

	if (nIrqCycles < nCpsCycles * nFirstLine / 0x0106) {
		SekRun(nIrqCycles);
		DoIRQ();
	}
	nNext = nCpsCycles * nFirstLine / 0x0106;
	if (SekTotalCycles() < nNext)
		SekRun(nNext - SekTotalCycles());

	CopyCpsReg(0);		// Get initial copy of registers
	CopyCpsFrg(0);

	if (nIrqLine >= 0x0106 && (*((unsigned short*)(CpsReg + 0x4E)) & 0x0200) == 0) {
		nIrqLine50 = *((unsigned short*)(CpsReg + 0x50)) & 0x01FF;
		nIrqLine52 = *((unsigned short*)(CpsReg + 0x52)) & 0x01FF;
		ScheduleIRQ();
	}

	GetPalette(0, 4);	// Get palettes
	Cps2ObjGet();		// Get objects

	for (i = 0; i < 3; i++)
	{
		nNext = ((i + 1) * nDisplayEnd) / 3;	// find out next cycle count to run to

		while (nNext > nIrqCycles && nInterrupt < MAX_RASTER)
		{
			SekRun(nIrqCycles - SekTotalCycles());
			DoIRQ();
		}
		SekRun(nNext - SekTotalCycles());	// run cpu
	}

	//	nCpsCyclesSegment[0] = (nCpsCycles * nVBlank) / 0x0106;
	//	nDone += SekRun(nCpsCyclesSegment[0] - nDone);

	SekSetIRQLine(2, SEK_IRQSTATUS_AUTO);	// VBlank
	SekRun(nCpsCycles - SekTotalCycles());

	if (pBurnDraw)
		Cps2Draw();

	nCpsCyclesExtra = SekTotalCycles() - nCpsCycles;

	QsndEndFrame();

	SekClose();

	//	bprintf(PRINT_NORMAL, _T("    -\n"));

	return 0;
}
