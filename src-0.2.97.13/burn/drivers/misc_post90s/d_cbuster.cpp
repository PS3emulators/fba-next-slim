// FB Alpha Crude Buster / Two Crude drive module
// Based on MAME driver by by Bryan McPhail

#include "tiles_generic.h"
#include "bitswap.h"
#include "deco16ic.h"
#include "burn_ym2203.h"
#include "burn_ym2151.h"
#include "msm6295.h"

static unsigned char *AllMem;
static unsigned char *MemEnd;
static unsigned char *AllRam;
static unsigned char *RamEnd;
static unsigned char *Drv68KROM;
static unsigned char *DrvHucROM;
static unsigned char *DrvGfxROM0;
static unsigned char *DrvGfxROM1;
static unsigned char *DrvGfxROM2;
static unsigned char *DrvGfxROM3;
static unsigned char *DrvSndROM0;
static unsigned char *DrvSndROM1;
static unsigned char *Drv68KRAM;
static unsigned char *DrvPalRAM0;
static unsigned char *DrvPalRAM1;
static unsigned char *DrvSprRAM;
static unsigned char *DrvSprBuf;

static unsigned int  *DrvPalette;
static unsigned char DrvRecalc;

static unsigned char *soundlatch;
static unsigned char *flipscreen;

static unsigned char DrvJoy1[16];
static unsigned char DrvJoy2[16];
static unsigned char DrvDips[2];
static unsigned char DrvReset;
static unsigned short DrvInputs[2];

static int prot_data = 0;

static struct BurnInputInfo CbusterInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Cbuster)

static struct BurnDIPInfo CbusterDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x00, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x00, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x40, 0x40, "Off"			},
	{0x13, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    0, "Lives"		},
	{0x14, 0x01, 0x03, 0x01, "1"			},
	{0x14, 0x01, 0x03, 0x00, "2"			},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x02, "4"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x14, 0x01, 0x0c, 0x08, "Easy"			},
	{0x14, 0x01, 0x0c, 0x04, "Hard"			},
	{0x14, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Allow Continue"	},
	{0x14, 0x01, 0x40, 0x00, "No"			},
	{0x14, 0x01, 0x40, 0x40, "Yes"			},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Cbuster)

void __fastcall cbuster_main_write_word(unsigned int address, unsigned short data)
{
	deco16_write_control_word(0, address, 0xb5000, data)
	deco16_write_control_word(1, address, 0xb6000, data)

	switch (address)
	{
		case 0xbc000:
		case 0xbc001:
			memcpy (DrvSprBuf, DrvSprRAM, 0x800);
		return;

		case 0xbc002:
		case 0xbc003:
			*soundlatch = data;
			//cpu_set_input_line(state->audiocpu, 0, HOLD_LINE);
		return;
	}
}

void __fastcall cbuster_main_write_byte(unsigned int address, unsigned char data)
{
	switch (address)
	{
		case 0xbc000:
		case 0xbc001:
			memcpy (DrvSprBuf, DrvSprRAM, 0x800);
		return;

		case 0xbc002:
		case 0xbc003:
			*soundlatch = data;
			//cpu_set_input_line(state->audiocpu, 0, HOLD_LINE);
		return;

		case 0xbc004:
			if (data == 0x9a)   prot_data = 0;
			if (data == 0x02)   prot_data = 0x63 << 8;
			if (data == 0x00) { prot_data = 0x0e; deco16_priority = 0; }
		return;

		case 0xbc005:
			if (data == 0xaa)   prot_data = 0x74;
			if (data == 0x9a)   prot_data = 0x0e;
			if (data == 0x55)   prot_data = 0x1e;
			if (data == 0x0e) { prot_data = 0x0e; deco16_priority = 0; }
			if (data == 0x00) { prot_data = 0x0e; deco16_priority = 0; }
			if (data == 0xf1) { prot_data = 0x36; deco16_priority = 1; }
			if (data == 0x80) { prot_data = 0x2e; deco16_priority = 1; }
			if (data == 0x40) { prot_data = 0x1e; deco16_priority = 1; }
			if (data == 0xc0) { prot_data = 0x3e; deco16_priority = 0; }
			if (data == 0xff) { prot_data = 0x76; deco16_priority = 1; }
		return;
	}
}

unsigned short __fastcall cbuster_main_read_word(unsigned int address)
{
	switch (address)
	{
		case 0xbc000:
		case 0xbc001:
			return DrvInputs[0];

		case 0xbc002:
		case 0xbc003:
			return (DrvDips[1] << 8) | (DrvDips[0] << 0);

		case 0xbc004:
			bprintf (0, _T("%5.5x, rw\n"), address);
			return prot_data;

		case 0xbc006:
		case 0xbc007:
			return (DrvInputs[1] & 0xf7) | (deco16_vblank & 0x08);
	}

	return 0;
}

unsigned char __fastcall cbuster_main_read_byte(unsigned int address)
{
	switch (address)
	{
		case 0xbc000:
			return DrvInputs[0] >> 8;

		case 0xbc001:
			return DrvInputs[0];

		case 0xbc002:
			return DrvDips[1];

		case 0xbc003:
			return DrvDips[0];

		case 0xbc004:
			bprintf (0, _T("%5.5x, rb\n"), address);
			return prot_data >> 8;
		case 0xbc005:
			bprintf (0, _T("%5.5x, rb\n"), address);
			return prot_data;

		case 0xbc006:
		case 0xbc007:
			return (DrvInputs[1] & 0xf7) | (deco16_vblank & 0x08);
	}

	return 0;
}

static int cbuster_bank_callback(const int bank)
{
	return ((bank >> 4) & 0x7) * 0x1000;
}

static void DrvYM2151IrqHandler(int state)
{
	state = state; // kill warnings...
//	HucSetIRQLine(1, state ? HUC_IRQSTATUS_ACK : HUC_IRQSTATUS_NONE);
}

static int DrvSynchroniseStream(int nSoundRate)
{
	return 0 * nSoundRate; //(long long)HucTotalCycles() * nSoundRate / 8055000;
}

static double DrvGetTime()
{
	return 0; //(double)HucTotalCycles() / 8055000.0;
}

static int DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	// Huc6280

	BurnYM2151Reset();
	BurnYM2203Reset();
	MSM6295Reset(0);
	MSM6295Reset(1);

	deco16Reset();

	return 0;
}

static int DrvGfxDecode(unsigned char *gfx, int len, int type)
{
	int Plane[4] = { 24,16,8,0 };
	int XOffs[16] = { 512,513,514,515,516,517,518,519, 0, 1, 2, 3, 4, 5, 6, 7 };
	int YOffs[16] = { 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,  8*32, 9*32,10*32,11*32,12*32,13*32,14*32,15*32};

	unsigned char *tmp = (unsigned char*)malloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, gfx, len);

	if (type == 1) {
		GfxDecode((len * 2) / 0x040, 4,  8,  8, Plane, XOffs + 8, YOffs, 0x100, tmp, gfx);
	} else {
		GfxDecode((len * 2) / 0x100, 4, 16, 16, Plane, XOffs + 0, YOffs, 0x400, tmp, gfx);
	}

	if (tmp) {
		free (tmp);
		tmp = NULL;
	}

	return 0;
}

static int MemIndex()
{
	unsigned char *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;
	DrvHucROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x200000;
	DrvGfxROM1	= Next; Next += 0x200000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x400000;

	MSM6295ROM	= Next;
	DrvSndROM0	= Next; Next += 0x040000;
	DrvSndROM1	= Next; Next += 0x040000;

	DrvPalette	= (unsigned int*)Next; Next += 0x0800 * sizeof(int);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvSprRAM	= Next; Next += 0x000800;
	DrvSprBuf	= Next; Next += 0x000800;
	DrvPalRAM0	= Next; Next += 0x001000;
	DrvPalRAM1	= Next; Next += 0x001000;

	soundlatch	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvROMRearrange()
{
	unsigned char *RAM = Drv68KROM;
	unsigned char *PTR;

	for (int i = 0; i < 0x80000; i += 2)
	{
		int h = i + 1, l = i + 0;
		RAM[h] = BITSWAP08(RAM[h], 4, 6, 7, 5, 3, 2, 1, 0);
		RAM[l] = BITSWAP08(RAM[l], 7, 1, 5, 4, 6, 2, 3, 0);

#if 0
		RAM[h] = (RAM[h] & 0xcf) | ((RAM[h] & 0x10) << 1) | ((RAM[h] & 0x20) >> 1);
		RAM[h] = (RAM[h] & 0x5f) | ((RAM[h] & 0x20) << 2) | ((RAM[h] & 0x80) >> 2);

		RAM[l] = (RAM[l] & 0xbd) | ((RAM[l] & 0x2) << 5) | ((RAM[l] & 0x40) >> 5);
		RAM[l] = (RAM[l] & 0xf5) | ((RAM[l] & 0x2) << 2) | ((RAM[l] & 0x8) >> 2);
#endif
	}

	RAM = DrvGfxROM3 + 0x080000;
	PTR = DrvGfxROM3 + 0x140000;
	for (int i = 0; i < 0x20000; i += 64)
	{
		for (int j = 0; j < 16; j++)
		{
			RAM[i + 0x00000 + j * 2] = PTR[i / 2 + 0x00000 + j];
			RAM[i + 0x00020 + j * 2] = PTR[i / 2 + 0x00010 + j];
			RAM[i + 0x00001 + j * 2] = PTR[i / 2 + 0x10000 + j];
			RAM[i + 0x00021 + j * 2] = PTR[i / 2 + 0x10010 + j];
			RAM[i + 0xa0000 + j * 2] = PTR[i / 2 + 0x20000 + j];
			RAM[i + 0xa0020 + j * 2] = PTR[i / 2 + 0x20010 + j];
			RAM[i + 0xa0001 + j * 2] = PTR[i / 2 + 0x30000 + j];
			RAM[i + 0xa0021 + j * 2] = PTR[i / 2 + 0x30010 + j];
		}
	}
}

static int DrvInit()
{
	BurnSetRefreshRate(58.00);

	AllMem = NULL;
	MemIndex();
	int nLen = MemEnd - (unsigned char *)0;
	if ((AllMem = (unsigned char *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000,  3, 2)) return 1;

		if (BurnLoadRom(DrvHucROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080001,  7, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x0a0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x140000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x150000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x160000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x170000, 14, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 15, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 16, 1)) return 1;

		DrvROMRearrange();

		memcpy (DrvGfxROM0, DrvGfxROM1, 0x100000);

		DrvGfxDecode(DrvGfxROM0, 0x100000, 1);
		DrvGfxDecode(DrvGfxROM1, 0x100000, 0);
		DrvGfxDecode(DrvGfxROM2, 0x080000, 0);

		deco16_tile_decode(DrvGfxROM3, DrvGfxROM3, 0x0a0000 * 2, 0);
	}

	deco16Init(0, 0, 1);
	deco16_set_global_offsets(0, 8);
	deco16_set_graphics(DrvGfxROM0, 0x200000, DrvGfxROM1, 0x200000, DrvGfxROM2, 0x100000);
	deco16_set_bank_callback(0, cbuster_bank_callback);
	deco16_set_bank_callback(1, cbuster_bank_callback);
	deco16_set_bank_callback(2, cbuster_bank_callback);
	deco16_set_bank_callback(3, cbuster_bank_callback);
	deco16_set_color_base(0, 0x000);
	deco16_set_color_base(1, 0x200);
	deco16_set_color_base(2, 0x300);
	deco16_set_color_base(3, 0x400);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, SM_ROM);
	SekMapMemory(Drv68KRAM,			0x080000, 0x083fff, SM_RAM);
	SekMapMemory(deco16_pf_ram[0],		0x0a0000, 0x0a1fff, SM_RAM);
	SekMapMemory(deco16_pf_ram[1],		0x0a2000, 0x0a2fff, SM_RAM);
	SekMapMemory(deco16_pf_rowscroll[0],	0x0a4000, 0x0a47ff, SM_RAM);
	SekMapMemory(deco16_pf_rowscroll[1],	0x0a6000, 0x0a67ff, SM_RAM);
	SekMapMemory(deco16_pf_ram[2],		0x0a8000, 0x0a8fff, SM_RAM);
	SekMapMemory(deco16_pf_ram[3],		0x0aa000, 0x0abfff, SM_RAM);
	SekMapMemory(deco16_pf_rowscroll[2],	0x0ac000, 0x0ac7ff, SM_RAM);
	SekMapMemory(deco16_pf_rowscroll[3],	0x0ae000, 0x0ae7ff, SM_RAM);
	SekMapMemory(DrvSprRAM,			0x0b0000, 0x0b07ff, SM_RAM);
	SekMapMemory(DrvPalRAM0,		0x0b8000, 0x0b8fff, SM_RAM);
	SekMapMemory(DrvPalRAM1,		0x0b9000, 0x0b9fff, SM_RAM);
	SekSetWriteWordHandler(0,		cbuster_main_write_word);
	SekSetWriteByteHandler(0,		cbuster_main_write_byte);
	SekSetReadWordHandler(0,		cbuster_main_read_word);
	SekSetReadByteHandler(0,		cbuster_main_read_byte);
	SekClose();

	// Huc6280...

	BurnYM2203Init(1, 4027500, NULL, DrvSynchroniseStream, DrvGetTime, 1);
//	BurnTimerAttachHuc(8055000);

	BurnYM2151Init(3580000, 45.0);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);

	MSM6295Init(0, 1023924 / 132, 75.0, 1);
	MSM6295Init(1, 2047848 / 132, 60.0, 1);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static int DrvExit()
{
	GenericTilesExit();
	deco16Exit();

	BurnYM2151Exit();
	BurnYM2203Exit();
	MSM6295Exit(0);
	MSM6295Exit(1);

	SekExit();
//	huc6280

	if (AllMem) {
		free (AllMem);
		AllMem = NULL;
	}

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteRecalc()
{
	unsigned short *p0 = (unsigned short*)DrvPalRAM0;
	unsigned short *p1 = (unsigned short*)DrvPalRAM1;

	for (int i = 0; i < BurnDrvGetPaletteEntries(); i++) {
		int r = ((p0[i] & 0xff) * 175) / 100;
		int g = ((p0[i] >>  8 ) * 175) / 100;
		int b = ((p1[i] & 0xff) * 175) / 100;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites(int pri)
{
	unsigned short *buffered_spriteram = (unsigned short*)DrvSprBuf;

	for (int offs = 0; offs < 0x400; offs += 4)
	{
		int x, y, sprite, colour, multi, fx, fy, inc, flash, mult;

		sprite = buffered_spriteram[offs + 1] & 0x7fff;
		if (!sprite)
			continue;

		y = buffered_spriteram[offs];
		x = buffered_spriteram[offs + 2];

		if ((y & 0x8000) && pri == 1)
			continue;
		if (!(y & 0x8000) && pri == 0)
			continue;

		colour = (x >> 9) & 0xf;
		if (x & 0x2000)
			colour += 64;
		colour += 0x10;

		flash = y & 0x1000;
		if (flash && (nCurrentFrame & 1))
			continue;

		fx = y & 0x2000;
		fy = y & 0x4000;
		multi = (1 << ((y & 0x0600) >> 9)) - 1;

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 256) x -= 512;
		if (y >= 256) y -= 512;
		x = 240 - x;
		y = 240 - y;

		if (x > 256) continue;

		sprite &= ~multi;
		if (fy)
			inc = -1;
		else
		{
			sprite += multi;
			inc = 1;
		}

		if (*flipscreen)
		{
			y = 240 - y;
			x = 240 - x;
			if (fx) fx = 0; else fx = 1;
			if (fy) fy = 0; else fy = 1;
			mult = 16;
		}
		else mult = -16;

		while (multi >= 0)
		{
			int sy = (y + mult * multi) - 8;
			int c = sprite - multi * inc;

			if (fy) {
				if (fx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, c, x, sy, colour, 4, 0, 0, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, c, x, sy, colour, 4, 0, 0, DrvGfxROM3);
				}
			} else {
				if (fx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, c, x, sy, colour, 4, 0, 0, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, c, x, sy, colour, 4, 0, 0, DrvGfxROM3);
				}
			}

			multi--;
		}
	}
}

static int DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteRecalc();
		DrvRecalc = 0;
//	}

	deco16_pf12_update();
	deco16_pf34_update();

	for (int i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x100;
	}

	if (nBurnLayer & 1) deco16_draw_layer(3, pTransDraw, DECO16_LAYER_OPAQUE);

	draw_sprites(0);

	if (deco16_priority) {
		if (nBurnLayer & 2) deco16_draw_layer(1, pTransDraw, 0);
		if (nBurnLayer & 4) deco16_draw_layer(2, pTransDraw, 0);
	} else {
		if (nBurnLayer & 2) deco16_draw_layer(2, pTransDraw, 0);
		if (nBurnLayer & 4) deco16_draw_layer(1, pTransDraw, 0);
	}

	draw_sprites(1);

	if (nBurnLayer & 8) deco16_draw_layer(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static int DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2 * sizeof(short)); 
		for (int i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	int nInterleave = 256;
	int nCyclesTotal[2] = { 12000000 / 58, 8055000 / 58 };
	int nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
//	HucOpen(0);

	deco16_vblank = 0;

	for (int i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
//		nCyclesDone[1] += HucRun(nCyclesTotal[1] / nInterleave);

		if (i == 240) deco16_vblank = 0x08;
	}

	SekSetIRQLine(4, SEK_IRQSTATUS_AUTO);

//	HucClose();
	SekClose();

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static int DrvScan(int nAction, int *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029682;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
	//	huc6280

		deco16Scan();

		BurnYM2151Scan(nAction);
		BurnYM2203Scan(nAction, pnMin);
		MSM6295Scan(0, nAction);
		MSM6295Scan(1, nAction);
	}

	return 0;
}


// Crude Buster (World FX version)

static struct BurnRomInfo cbusterRomDesc[] = {
	{ "fx01.rom",		0x20000, 0xddae6d83, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fx00.rom",		0x20000, 0x5bc2c0de, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fx03.rom",		0x20000, 0xc3d65bf9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fx02.rom",		0x20000, 0xb875266b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fu11-.rom",		0x10000, 0x65f20f10, 2 | BRF_PRG | BRF_ESS }, //  4 Huc6280 Code

	{ "mab-00",		0x80000, 0x660eaabd, 3 | BRF_GRA },           //  5 Characters and Foreground Tiles
	{ "fu05-.rom",		0x10000, 0x8134d412, 3 | BRF_GRA },           //  6
	{ "fu06-.rom",		0x10000, 0x2f914a45, 3 | BRF_GRA },           //  7

	{ "mab-01",		0x80000, 0x1080d619, 4 | BRF_GRA },           //  8 Background Tiles

	{ "mab-02",		0x80000, 0x58b7231d, 5 | BRF_GRA },           //  9 Sprites
	{ "mab-03",		0x80000, 0x76053b9d, 5 | BRF_GRA },           // 10
	{ "fu07-.rom",		0x10000, 0xca8d0bb3, 5 | BRF_GRA },           // 11
	{ "fu08-.rom",		0x10000, 0xc6afc5c8, 5 | BRF_GRA },           // 12
	{ "fu09-.rom",		0x10000, 0x526809ca, 5 | BRF_GRA },           // 13
	{ "fu10-.rom",		0x10000, 0x6be6d50e, 5 | BRF_GRA },           // 14

	{ "fu12-.rom",		0x20000, 0x2d1d65f2, 6 | BRF_SND },           // 15 OKI M6295 Samples 0

	{ "fu13-.rom",		0x20000, 0xb8525622, 7 | BRF_SND },           // 16 OKI M6295 Samples 1

	{ "mb7114h.18e",	0x00100, 0x3645b70f, 8 | BRF_OPT },           // 17 Unused PROMs
};

STD_ROM_PICK(cbuster)
STD_ROM_FN(cbuster)

struct BurnDriver BurnDrvCbuster = {
	"cbuster", NULL, NULL, NULL, "1990",
	"Crude Buster (World FX version)\0","No sound", "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, NULL, cbusterRomInfo, cbusterRomName, NULL, NULL, CbusterInputInfo, CbusterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// Crude Buster (World FU version)

static struct BurnRomInfo cbusterwRomDesc[] = {
	{ "fu01-.rom",		0x20000, 0x0203e0f8, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fu00-.rom",		0x20000, 0x9c58626d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fu03-.rom",		0x20000, 0xdef46956, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fu02-.rom",		0x20000, 0x649c3338, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fu11-.rom",		0x10000, 0x65f20f10, 2 | BRF_PRG | BRF_ESS }, //  4 Huc6280 Code

	{ "mab-00",		0x80000, 0x660eaabd, 3 | BRF_GRA },           //  5 Characters and Foreground Tiles
	{ "fu05-.rom",		0x10000, 0x8134d412, 3 | BRF_GRA },           //  6
	{ "fu06-.rom",		0x10000, 0x2f914a45, 3 | BRF_GRA },           //  7

	{ "mab-01",		0x80000, 0x1080d619, 4 | BRF_GRA },           //  8 Background Tiles

	{ "mab-02",		0x80000, 0x58b7231d, 5 | BRF_GRA },           //  9 Sprites
	{ "mab-03",		0x80000, 0x76053b9d, 5 | BRF_GRA },           // 10
	{ "fu07-.rom",		0x10000, 0xca8d0bb3, 5 | BRF_GRA },           // 11
	{ "fu08-.rom",		0x10000, 0xc6afc5c8, 5 | BRF_GRA },           // 12
	{ "fu09-.rom",		0x10000, 0x526809ca, 5 | BRF_GRA },           // 13
	{ "fu10-.rom",		0x10000, 0x6be6d50e, 5 | BRF_GRA },           // 14

	{ "fu12-.rom",		0x20000, 0x2d1d65f2, 6 | BRF_SND },           // 15 OKI M6295 Samples 0

	{ "fu13-.rom",		0x20000, 0xb8525622, 7 | BRF_SND },           // 16 OKI M6295 Samples 1

	{ "mb7114h.18e",	0x00100, 0x3645b70f, 8 | BRF_OPT },           // 17 Unused PROMs
};

STD_ROM_PICK(cbusterw)
STD_ROM_FN(cbusterw)

struct BurnDriver BurnDrvCbusterw = {
	"cbusterw", "cbuster", NULL, NULL, "1990",
	"Crude Buster (World FU version)\0", "No sound", "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, NULL, cbusterwRomInfo, cbusterwRomName, NULL, NULL, CbusterInputInfo, CbusterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// Crude Buster (Japan)

static struct BurnRomInfo cbusterjRomDesc[] = {
	{ "fr01-1",		0x20000, 0xaf3c014f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fr00-1",		0x20000, 0xf666ad52, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fr03",		0x20000, 0x02c06118, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fr02",		0x20000, 0xb6c34332, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fu11-.rom",		0x10000, 0x65f20f10, 2 | BRF_PRG | BRF_ESS }, //  4 Huc6280 Code

	{ "mab-00",		0x80000, 0x660eaabd, 3 | BRF_GRA },           //  5 Characters and Foreground Tiles
	{ "fu05-.rom",		0x10000, 0x8134d412, 3 | BRF_GRA },           //  6
	{ "fu06-.rom",		0x10000, 0x2f914a45, 3 | BRF_GRA },           //  7

	{ "mab-01",		0x80000, 0x1080d619, 4 | BRF_GRA },           //  8 Background Tiles

	{ "mab-02",		0x80000, 0x58b7231d, 5 | BRF_GRA },           //  9 Sprites
	{ "mab-03",		0x80000, 0x76053b9d, 5 | BRF_GRA },           // 10
	{ "fr07",		0x10000, 0x52c85318, 5 | BRF_GRA },           // 11
	{ "fr08",		0x10000, 0xea25fbac, 5 | BRF_GRA },           // 12
	{ "fr09",		0x10000, 0xf8363424, 5 | BRF_GRA },           // 13
	{ "fr10",		0x10000, 0x241d5760, 5 | BRF_GRA },           // 14

	{ "fu12-.rom",		0x20000, 0x2d1d65f2, 6 | BRF_SND },           // 15 OKI M6295 Samples 0

	{ "fu13-.rom",		0x20000, 0xb8525622, 7 | BRF_SND },           // 16 OKI M6295 Samples 1

	{ "mb7114h.18e",	0x00100, 0x3645b70f, 8 | BRF_OPT },           // 17 Unused PROMs
};

STD_ROM_PICK(cbusterj)
STD_ROM_FN(cbusterj)

struct BurnDriver BurnDrvCbusterj = {
	"cbusterj", "cbuster", NULL, NULL, "1990",
	"Crude Buster (Japan)\0", "No sound", "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, NULL, cbusterjRomInfo, cbusterjRomName, NULL, NULL, CbusterInputInfo, CbusterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// Two Crude (US)

static struct BurnRomInfo twocrudeRomDesc[] = {
	{ "ft01",		0x20000, 0x08e96489, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ft00",		0x20000, 0x6765c445, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ft03",		0x20000, 0x28002c99, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ft02",		0x20000, 0x37ea0626, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fu11-.rom",		0x10000, 0x65f20f10, 2 | BRF_PRG | BRF_ESS }, //  4 Huc6280 Code

	{ "mab-00",		0x80000, 0x660eaabd, 3 | BRF_GRA },           //  5 Characters and Foreground Tiles
	{ "fu05-.rom",		0x10000, 0x8134d412, 3 | BRF_GRA },           //  6
	{ "fu06-.rom",		0x10000, 0x2f914a45, 3 | BRF_GRA },           //  7

	{ "mab-01",		0x80000, 0x1080d619, 4 | BRF_GRA },           //  8 Background Tiles

	{ "mab-02",		0x80000, 0x58b7231d, 5 | BRF_GRA },           //  9 Sprites
	{ "mab-03",		0x80000, 0x76053b9d, 5 | BRF_GRA },           // 10
	{ "ft07",		0x10000, 0xe3465c25, 5 | BRF_GRA },           // 11
	{ "ft08",		0x10000, 0xc7f1d565, 5 | BRF_GRA },           // 12
	{ "ft09",		0x10000, 0x6e3657b9, 5 | BRF_GRA },           // 13
	{ "ft10",		0x10000, 0xcdb83560, 5 | BRF_GRA },           // 14

	{ "fu12-.rom",		0x20000, 0x2d1d65f2, 6 | BRF_SND },           // 15 OKI M6295 Samples 0

	{ "fu13-.rom",		0x20000, 0xb8525622, 7 | BRF_SND },           // 16 OKI M6295 Samples 1

	{ "mb7114h.18e",	0x00100, 0x3645b70f, 8 | BRF_OPT },           // 17 Unused PROMs
};

STD_ROM_PICK(twocrude)
STD_ROM_FN(twocrude)

struct BurnDriver BurnDrvTwocrude = {
	"twocrude", "cbuster", NULL, NULL, "1990",
	"Two Crude (US)\0", "No sound", "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM | GBF_SCRFIGHT, 0,
	NULL, NULL, twocrudeRomInfo, twocrudeRomName, NULL, NULL, CbusterInputInfo, CbusterDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};
