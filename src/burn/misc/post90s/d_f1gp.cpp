// FB Alpha F-1 Grand Prix driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "burn_ym2610.h"

static unsigned char *AllMem;
static unsigned char *MemEnd;
static unsigned char *AllRam;
static unsigned char *RamEnd;
static unsigned char *Drv68KROM0;
static unsigned char *Drv68KROM1;
static unsigned char *DrvZ80ROM;
static unsigned char *DrvGfxROM0;
static unsigned char *DrvGfxROM1;
static unsigned char *DrvGfxROM2;
static unsigned char *DrvGfxROM3;
static unsigned char *DrvSndROM;
static unsigned char *Drv68KRAM0;
static unsigned char *Drv68KRAM1;
static unsigned char *DrvPalRAM;
static unsigned char *DrvZoomRAM;
static unsigned char *DrvRozVidRAM;
static unsigned char *DrvVidRAM;
static unsigned char *DrvShareRAM;
static unsigned char *DrvSprVRAM1;
static unsigned char *DrvSprVRAM2;
static unsigned char *DrvSprCGRAM1;
static unsigned char *DrvSprCGRAM2;
static unsigned char *DrvZ80RAM;
static unsigned char *DrvBgDirty;
static unsigned char *DrvBgTileDirty;

static unsigned int *DrvPalette;
static unsigned char DrvRecalc;

static unsigned short *DrvBgTmp;

static unsigned char *soundlatch;
static unsigned char *flipscreen;
static unsigned char *gfxctrl;
static unsigned char *pending_command;
static unsigned char *roz_bank;
static unsigned char *DrvZ80Bank;
static unsigned short *DrvFgScrollX;
static unsigned short *DrvFgScrollY;
static unsigned short *DrvBgCtrl;

static unsigned char DrvJoy1[16];
static unsigned char DrvDips[4];
static unsigned short DrvInputs[3];
static unsigned char DrvReset;

static short zoom_table[32][33];

static int nScreenStartY = 8;

static struct BurnInputInfo F1gpInputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Region",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(F1gp)

static struct BurnInputInfo F1gp2InputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Region",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(F1gp2)

static struct BurnDIPInfo F1gpDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL				},
	{0x0a, 0xff, 0xff, 0xff, NULL				},
	{0x0b, 0xff, 0xff, 0xfb, NULL				},
	{0x0c, 0xff, 0xff, 0x10, NULL				},

	{0   , 0xfe, 0   ,    2, "Free Play"	},
	{0x0a, 0x01, 0x01, 0x01, "Off"				},
	{0x0a, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x0a, 0x01, 0x0e, 0x0a, "3 Coins 1 Credit"		  },
	{0x0a, 0x01, 0x0e, 0x0c, "2 Coins 1 Credit"		  },
	{0x0a, 0x01, 0x0e, 0x0e, "1 Coin  1 Credit"		  },
	{0x0a, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x0e, 0x06, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0x0e, 0x04, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0x0e, 0x02, "1 Coin  5 Credits"		},
	{0x0a, 0x01, 0x0e, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x0a, 0x01, 0x70, 0x50, "3 Coins 1 Credit"		  },
	{0x0a, 0x01, 0x70, 0x60, "2 Coins 1 Credit"		  },
	{0x0a, 0x01, 0x70, 0x70, "1 Coin  1 Credit"		  },
	{0x0a, 0x01, 0x70, 0x40, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x70, 0x30, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0x70, 0x20, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0x70, 0x10, "1 Coin  5 Credits"		},
	{0x0a, 0x01, 0x70, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "2 Coins to Start, 1 to Continue"	},
	{0x0a, 0x01, 0x80, 0x80, "Off"			},
	{0x0a, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x09, 0x01, 0x03, 0x02, "Easy"				},
	{0x09, 0x01, 0x03, 0x03, "Normal"			},
	{0x09, 0x01, 0x03, 0x01, "Hard"				},
	{0x09, 0x01, 0x03, 0x00, "Hardest"		},

//	{0   , 0xfe, 0   ,    2, "Game Mode"	},
//	{0x09, 0x01, 0x04, 0x04, "Single"			},
//	{0x09, 0x01, 0x04, 0x00, "Multiple"		},  // ID CHECK ERROR

	{0   , 0xfe, 0   ,    2, "Mode Select"			},
	{0x09, 0x01, 0x08, 0x00, "Off"		},
	{0x09, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0b, 0x01, 0x01, 0x01, "Off"					},
	{0x0b, 0x01, 0x01, 0x00, "On"					  },

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x0b, 0x01, 0x02, 0x02, "Off"			},
//	{0x0b, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0b, 0x01, 0x04, 0x04, "Off"				},
	{0x0b, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    6, "Country"			},
	{0x0c, 0x01, 0x1f, 0x10, "World"			},
	{0x0c, 0x01, 0x1f, 0x01, "USA & Canada"},
	{0x0c, 0x01, 0x1f, 0x00, "Japan"			},
	{0x0c, 0x01, 0x1f, 0x02, "Korea"			},
	{0x0c, 0x01, 0x1f, 0x04, "Hong Kong"			},
	{0x0c, 0x01, 0x1f, 0x08, "Taiwan"			},
};

STDDIPINFO(F1gp)

static struct BurnDIPInfo F1gp2DIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL				},
	{0x0b, 0xff, 0xff, 0xff, NULL				},
	{0x0c, 0xff, 0xff, 0xfb, NULL				},
	{0x0d, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    2, "Free Play"	},
	{0x0b, 0x01, 0x01, 0x01, "Off"				},
	{0x0b, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x0b, 0x01, 0x0e, 0x0a, "3 Coins 1 Credit"		  },
	{0x0b, 0x01, 0x0e, 0x0c, "2 Coins 1 Credit"		  },
	{0x0b, 0x01, 0x0e, 0x0e, "1 Coin  1 Credit"		  },
	{0x0b, 0x01, 0x0e, 0x08, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x0e, 0x06, "1 Coin  3 Credits"		},
	{0x0b, 0x01, 0x0e, 0x04, "1 Coin  4 Credits"		},
	{0x0b, 0x01, 0x0e, 0x02, "1 Coin  5 Credits"		},
	{0x0b, 0x01, 0x0e, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x0b, 0x01, 0x70, 0x50, "3 Coins 1 Credit"		  },
	{0x0b, 0x01, 0x70, 0x60, "2 Coins 1 Credit"		  },
	{0x0b, 0x01, 0x70, 0x70, "1 Coin  1 Credit"		  },
	{0x0b, 0x01, 0x70, 0x40, "1 Coin  2 Credits"		},
	{0x0b, 0x01, 0x70, 0x30, "1 Coin  3 Credits"		},
	{0x0b, 0x01, 0x70, 0x20, "1 Coin  4 Credits"		},
	{0x0b, 0x01, 0x70, 0x10, "1 Coin  5 Credits"		},
	{0x0b, 0x01, 0x70, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "2 Coins to Start, 1 to Continue"	},
	{0x0b, 0x01, 0x80, 0x80, "Off"			},
	{0x0b, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0a, 0x01, 0x03, 0x02, "Easy"				},
	{0x0a, 0x01, 0x03, 0x03, "Normal"			},
	{0x0a, 0x01, 0x03, 0x01, "Hard"				},
	{0x0a, 0x01, 0x03, 0x00, "Hardest"		},

//	{0   , 0xfe, 0   ,    2, "Game Mode"	},
//	{0x0a, 0x01, 0x04, 0x04, "Single"			},
//	{0x0a, 0x01, 0x04, 0x00, "Multiple"		},  // ID CHECK ERROR

	{0   , 0xfe, 0   ,    2, "Mode Select"			},
	{0x0a, 0x01, 0x08, 0x00, "Off"		},
	{0x0a, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0c, 0x01, 0x01, 0x01, "Off"					},
	{0x0c, 0x01, 0x01, 0x00, "On"					  },

//	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
//	{0x0c, 0x01, 0x02, 0x02, "Off"			},
//	{0x0c, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0c, 0x01, 0x04, 0x04, "Off"				},
	{0x0c, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Country"			},
	{0x0d, 0x01, 0x01, 0x01, "World"			},
	{0x0d, 0x01, 0x01, 0x00, "Japan"			},
};

STDDIPINFO(F1gp2)

static inline void expand_dynamic_tiles(int offset)
{
	unsigned short data = (*((unsigned short*)(DrvZoomRAM + offset)));

	offset <<= 1;

	DrvGfxROM3[offset + 0] = (swapWord(data) >> 12) & 0x0f;
	DrvGfxROM3[offset + 1] = (swapWord(data) >>  8) & 0x0f;
	DrvGfxROM3[offset + 2] = (swapWord(data) >>  4) & 0x0f;
	DrvGfxROM3[offset + 3] = (swapWord(data) >>  0) & 0x0f;

	DrvBgTileDirty[offset >> 8] = 1;
}

void __fastcall f1gp_main_write_word(unsigned int address, unsigned short data)
{
	if (((address & 0xfffffe0) == 0xfff040 && nScreenStartY == 8) ||	// f1gp
		((address & 0xffffff0) == 0xfff020 && nScreenStartY == 0)) {	// f1gp2
		DrvBgCtrl[(address >> 1) & 0x0f] = swapWord(data);
		return;
	}

	if ((address & 0xfc0000) == 0xc00000) {
		unsigned short *p = (unsigned short*)(DrvZoomRAM + (address & 0x3fffe));

		if (swapWord(p[0]) == swapWord(data)) return;

		p[0] = swapWord(data);

		expand_dynamic_tiles(address & 0x3fffe);

		return;
	}

	if ((address & 0xff8000) == 0xd00000) {
		unsigned short *p = (unsigned short*)(DrvRozVidRAM + (address & 0x1ffe));

		if (swapWord(p[0]) == swapWord(data)) return;

		p[0] = swapWord(data);

		DrvBgDirty[(address >> 1) & 0xfff] = 1;

		return;
	}

	switch (address)
	{
		case 0xfff002: // f1gp
		case 0xfff003:
			*DrvFgScrollX = data & 0x1ff;
		return;

		case 0xfff004:
		case 0xfff005:
			*DrvFgScrollY = data & 0x0ff;
		return;

		case 0xfff044: // f1gp2
		case 0xfff045:
			*DrvFgScrollX = (data+80) & 0x1ff;
		return;

		case 0xfff046:
		case 0xfff047:
			*DrvFgScrollY = (data+26) & 0x0ff;
		return;
	}
}

void __fastcall f1gp_main_write_byte(unsigned int address, unsigned char data)
{
	if ((address & 0xff8000) == 0xd00000) {
		address = (address & 0x1fff) ^ 1;

		int d = DrvRozVidRAM[address];

		if (d == data) return;

		DrvRozVidRAM[address] = data;
		DrvBgDirty[(address >> 1) & 0xfff] = 1;

		return;
	}

	switch (address)
	{
		case 0xfff000:
			if (*roz_bank != data) {
				*roz_bank = data;
				memset (DrvBgDirty,	1, 0x1000);
			}
		return;

		case 0xfff001:
			*flipscreen = data & 0x20;
			*gfxctrl = data & 0xdf;
		return;

		case 0xfff009:
		{
			int t = (SekTotalCycles() / 2) - ZetTotalCycles();
			if (t > 0) ZetRun(t);
			*pending_command = 0xff;
			*soundlatch = data;
			ZetNmi();
		}
		return;
	}
}

unsigned short __fastcall f1gp_main_read_word(unsigned int address)
{
	switch (address)
	{
		case 0xfff004:
			return (DrvDips[1] << 8) | DrvDips[0];

		case 0xfff006:
			return (DrvDips[2] << 8);
	}

	return 0;
}

unsigned char __fastcall f1gp_main_read_byte(unsigned int address)
{
	switch (address)
	{
		case 0xfff000:
		case 0xfff001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0xfff004:
			return DrvDips[0];

		case 0xfff005:
			return DrvDips[1];

		case 0xfff006:
			return DrvDips[2];

		case 0xfff009:
			return *pending_command;

		case 0xfff00b:
		case 0xfff051:
			return DrvDips[3];
	}

	return 0;
}

static void sound_bankswitch(int data)
{
	int nBank = ((data & 1) << 15) + 0x8000;

	*DrvZ80Bank = data & 1;

	ZetMapArea(0x8000, 0xffff, 0, DrvZ80ROM + nBank);
	ZetMapArea(0x8000, 0xffff, 2, DrvZ80ROM + nBank);
}

void __fastcall f1gp_sound_out(unsigned short port, unsigned char data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x0c:
			sound_bankswitch(data);
		return;

		case 0x14:
			*pending_command = 0;
		return;

		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
			BurnYM2610Write(port & 3, data);
		return;
	}
}

unsigned char __fastcall f1gp_sound_in(unsigned short port)
{
	switch (port & 0xff)
	{
		case 0x14:
			return *soundlatch;

		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
			return BurnYM2610Read(port & 3);
	}

	return 0;
}

static void DrvFMIRQHandler(int, int nStatus)
{
	if (nStatus) {
		ZetSetIRQLine(0xff, ZET_IRQSTATUS_ACK);
	} else {
		ZetSetIRQLine(0,    ZET_IRQSTATUS_NONE);
	}
}

static int DrvSynchroniseStream(int nSoundRate)
{
	return (long long)ZetTotalCycles() * nSoundRate / 5000000;
}

static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 5000000.0;
}

static int DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam,         0, RamEnd - AllRam);
	memset (DrvBgDirty,	1, 0x1000);
	memset (DrvBgTileDirty, 1, 0x0800);

	SekOpen(0);
	SekReset();
	SekClose();

	SekOpen(1);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2610Reset();

	return 0;
}

static int MemIndex()
{
	unsigned char *Next; Next = AllMem;

	Drv68KROM0	= Next; Next += 0x500000;
	Drv68KROM1	= Next; Next += 0x020000;
	DrvZ80ROM	= Next; Next += 0x020000;

	DrvGfxROM0	= Next; Next += 0x200000;
	DrvGfxROM1	= Next; Next += 0x400000;
	DrvGfxROM2	= Next; Next += 0x200000;
	DrvGfxROM3	= Next; Next += 0x800000;

	DrvSndROM	= Next; Next += 0x200000;

	DrvPalette	= (unsigned int  *)Next; Next += 0x000401 * sizeof(int);

	DrvBgDirty	= Next; Next += 0x001000;
	DrvBgTileDirty	= Next; Next += 0x000800;
	DrvBgTmp	= (unsigned short*)Next; Next += 0x100000 * sizeof(short);

	AllRam		= Next;

	Drv68KRAM0	= Next; Next += 0x004000;
	Drv68KRAM1	= Next; Next += 0x004000;
	DrvShareRAM	= Next; Next += 0x001000;
	DrvZoomRAM	= Next; Next += 0x040000;
	DrvPalRAM	= Next; Next += 0x001000;
	DrvRozVidRAM	= Next; Next += 0x002000;
	DrvVidRAM	= Next; Next += 0x001000;

	DrvSprVRAM1	= Next; Next += 0x001000;
	DrvSprVRAM2	= Next; Next += 0x000400;

	DrvSprCGRAM1	= Next; Next += 0x008000;
	DrvSprCGRAM2	= Next; Next += 0x004000;

	DrvZ80RAM	= Next; Next += 0x000800;

	pending_command	= Next; Next += 0x000001;
	soundlatch	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;
	gfxctrl		= Next; Next += 0x000001;
	roz_bank	= Next; Next += 0x000001;

	DrvZ80Bank	= Next; Next += 0x000001;

	DrvFgScrollX	= (unsigned short*)Next; Next += 0x000001 * sizeof(short);
	DrvFgScrollY	= (unsigned short*)Next; Next += 0x000001 * sizeof(short);
	DrvBgCtrl	= (unsigned short*)Next; Next += 0x000010 * sizeof(short);

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static void DrvInitZoomTable()
{
	zoom_table[0][0] = -1;

	for (int x = 1; x < 32; x++) {
		for (int y = 0; y < 16; y++) {
			float t = ((16.0000-1.0000) / x) * y;
			zoom_table[x][y] = (t >= 16) ? -1 : (int)t;
		}
	}
}

static void WordSwap(unsigned char *src, int len)
{
	for (int i = 0; i < len; i+=4) {
		int d = src[i+1];
		src[i+1] = src[i+2];
		src[i+2] = d;
	}
}

static int F1gpGfxDecode()
{
	int Plane[8]  = { 0x000, 0x001, 0x002, 0x003 };
	int XOffs[16] = { 0x004, 0x000, 0x00c, 0x008, 0x014, 0x010, 0x01c, 0x018,
			  0x024, 0x020, 0x02c, 0x028, 0x034, 0x030, 0x03c, 0x038 };
	int YOffs[16] = { 0x000, 0x040, 0x080, 0x0c0, 0x100, 0x140, 0x180, 0x1c0,
			  0x200, 0x240, 0x280, 0x2c0, 0x300, 0x340, 0x380, 0x3c0 };

	unsigned char *tmp = (unsigned char*)malloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM2);

	free (tmp);

	return 0;
}

static int F1gp2GfxDecode()
{
	int Plane[8]  = { 0x000, 0x001, 0x002, 0x003 };
	int XOffs[16] = { 0x008, 0x00c, 0x000, 0x004, 0x018, 0x01c, 0x010, 0x014,
			  0x028, 0x02c, 0x020, 0x024, 0x038, 0x03c, 0x030, 0x034 };
	int YOffs[16] = { 0x000, 0x040, 0x080, 0x0c0, 0x100, 0x140, 0x180, 0x1c0,
			  0x200, 0x240, 0x280, 0x2c0, 0x300, 0x340, 0x380, 0x3c0 };

	unsigned char *tmp = (unsigned char*)malloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	for (int i = 0; i < 0x200000; i++) {
		tmp[i^1] = (DrvGfxROM1[i] << 4) | (DrvGfxROM1[i] >> 4);
	}

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM3, 0x400000);

	GfxDecode(0x8000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM3);

	free (tmp);

	return 0;
}

static int DrvInit(int nGame)
{
	AllMem = NULL;
	MemIndex();
	int nLen = MemEnd - (unsigned char *)0;
	if ((AllMem = (unsigned char *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (nGame) {
		nScreenStartY = 0;

		if (BurnLoadRom(Drv68KROM0 + 0x0000001,	 0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0000000,	 1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0300000,	 2, 1)) return 1;
		memcpy (Drv68KROM0 + 0x100000, Drv68KROM0 + 0x400000, 0x100000);
		memcpy (Drv68KROM0 + 0x200000, Drv68KROM0 + 0x300000, 0x100000);

		if (BurnLoadRom(Drv68KROM1,		 3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM,		 4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,	 5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,	 6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000,	 7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x100000,	 8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x200000,	 9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x300000,	10, 1)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x0000000,	11, 1)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x0100000,  12, 1)) return 1;

		F1gp2GfxDecode();
	} else {
		if (BurnLoadRom(Drv68KROM0 + 0x0000000,	 0, 1)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0100000,	 1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0100001,	 2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0180000,	 3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0180001,	 4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0200000,	 5, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0200001,	 6, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0280000,	 7, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0280001,	 8, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0300000,	 9, 1)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0400000,	10, 1)) return 1;

		if (BurnLoadRom(Drv68KROM1,		11, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM,		12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,	13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000,	14, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,	15, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,	16, 2)) return 1;
		WordSwap(DrvGfxROM1, 0x100000);

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,	17, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,	18, 2)) return 1;
		WordSwap(DrvGfxROM2, 0x080000);

		if (BurnLoadRom(DrvSndROM + 0x0000000,	19, 1)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x0100000,  20, 1)) return 1;

		F1gpGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM0,		0x000000, 0x03ffff, SM_ROM);
	SekMapMemory(Drv68KROM0 + 0x100000,	0x100000, 0x2fffff, SM_ROM);
	SekMapMemory(DrvRozVidRAM,		0xd00000, 0xd01fff, SM_ROM);
	SekMapMemory(DrvRozVidRAM,		0xd02000, 0xd03fff, SM_ROM);
	SekMapMemory(DrvRozVidRAM,		0xd04000, 0xd05fff, SM_ROM);
	SekMapMemory(DrvRozVidRAM,		0xd06000, 0xd07fff, SM_ROM);

	if (nGame) {
		SekMapMemory(DrvSprCGRAM1,		0xa00000, 0xa07fff, SM_RAM);
		SekMapMemory(DrvSprVRAM1,		0xe00000, 0xe00fff, SM_RAM);
	} else {
		SekMapMemory(Drv68KROM0 + 0x300000,	0xa00000, 0xbfffff, SM_ROM);
		SekMapMemory(DrvZoomRAM,		0xc00000, 0xc3ffff, SM_ROM);
		SekMapMemory(DrvSprCGRAM1,		0xe00000, 0xe03fff, SM_RAM);
		SekMapMemory(DrvSprCGRAM2,		0xe04000, 0xe07fff, SM_RAM);
		SekMapMemory(DrvSprVRAM1,		0xf00000, 0xf003ff, SM_RAM);
		SekMapMemory(DrvSprVRAM2,		0xf10000, 0xf103ff, SM_RAM);
	}

	SekMapMemory(Drv68KRAM0,		0xff8000, 0xffbfff, SM_RAM);
	SekMapMemory(DrvShareRAM,		0xffc000, 0xffcfff, SM_RAM);
	SekMapMemory(DrvVidRAM,			0xffd000, 0xffdfff, SM_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, SM_RAM);
	SekSetWriteWordHandler(0,		f1gp_main_write_word);
	SekSetWriteByteHandler(0,		f1gp_main_write_byte);
	SekSetReadWordHandler(0,		f1gp_main_read_word);
	SekSetReadByteHandler(0,		f1gp_main_read_byte);
	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(Drv68KROM1,		0x000000, 0x01ffff, SM_ROM);
	SekMapMemory(Drv68KRAM1,		0xff8000, 0xffbfff, SM_RAM);
	SekMapMemory(DrvShareRAM,		0xffc000, 0xffcfff, SM_RAM);
	SekClose();

	ZetInit(1);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x77ff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x77ff, 2, DrvZ80ROM);
	ZetMapArea(0x7800, 0x7fff, 0, DrvZ80RAM);
	ZetMapArea(0x7800, 0x7fff, 1, DrvZ80RAM);
	ZetMapArea(0x7800, 0x7fff, 2, DrvZ80RAM);
	ZetMapArea(0x8000, 0xffff, 0, DrvZ80ROM + 0x8000);
	ZetMapArea(0x8000, 0xffff, 2, DrvZ80ROM + 0x8000);
	ZetSetOutHandler(f1gp_sound_out);
	ZetSetInHandler(f1gp_sound_in);
	ZetMemEnd();
	ZetClose();

	int DrvSndROMLen = 0x100000;
	BurnYM2610Init(8000000, DrvSndROM + 0x100000, &DrvSndROMLen, DrvSndROM, &DrvSndROMLen, &DrvFMIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(5000000);

	DrvInitZoomTable();
	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static int DrvExit()
{
	GenericTilesExit();

	BurnYM2610Exit();
	SekExit();
	ZetExit();

	free (AllMem);
	AllMem = NULL;

	nScreenStartY = 8;

	return 0;
}

static void draw_foreground(int transp)
{
	transp *= 0xff;

	unsigned short *vram = (unsigned short*)DrvVidRAM;

	for (int offs = nScreenStartY; offs < 64 * 32; offs++)
	{
		int sx = (offs & 0x3f) << 3;
		int sy = ((offs >> 6) << 3) - nScreenStartY;

		sx -= *DrvFgScrollX;
		if (sx < -7) sx += 512;
		sy -= *DrvFgScrollY;
		if (sy < -7) sy += 256;

		int code  = swapWord(vram[offs]) & 0x7fff;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		int flipy = swapWord(vram[offs]) & 0x8000;

		if (transp) {
			if (flipy) {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, 0, 8, transp, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, 0, 8, transp, 0, DrvGfxROM0);
			}
		} else {
			if (flipy) {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, 0, 8, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 8, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_16x16_zoom(unsigned char *gfx, int code, int color, int sx, int sy, int xz, int yz, int fx, int fy)
{
	if (yz <= 1 || xz <= 1) return;

	if (yz == 16 && xz == 16) {
		if (fy) {
			if (fx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0, gfx);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0, gfx);
			}
		} else {
			if (fx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0, gfx);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0, gfx);
			}
		}

		return;
	}

	fx = (fx * 0x0f);
	fy = (fy >> 1) * 0x0f;

	color = (color << 4);
	unsigned char *src = gfx + (code << 8);

	short *xm = zoom_table[xz-1];
	short *ym = zoom_table[yz-1];

	for (int y = 0; y < 16; y++)
	{
		int yy = sy + y;

		if (ym[y ^ fy] == -1 || yy < 0 || yy >= nScreenHeight) continue;

		int yyz = (ym[y ^ fy] << 4);

		for (int x = 0; x < 16; x++)
		{
			short xxz = xm[x ^ fx];
			if (xxz == -1) continue;

			int xx = sx + x;

			int pxl = src[yyz|xxz];

			if (pxl == 15 || xx < 0 || xx >= nScreenWidth || yy < 0 || yy >= nScreenHeight) continue;

			pTransDraw[(yy * nScreenWidth) + xx] = pxl | color;
		}
	}
}

static void f1gp_draw_sprites(unsigned char *ram0, unsigned char *ram1, unsigned char *gfx, int colo)
{
	int attr_start,first;
	unsigned short *spram = (unsigned short*)ram0;
	unsigned short *spram2 = (unsigned short*)ram1;

	static const int zoomtable[16] = { 0,7,14,20,25,30,34,38,42,46,49,52,54,57,59,61 };

	first = 4 * spram[0x1fe];

	for (attr_start = first; attr_start < 0x0200; attr_start += 4)
	{
		int map_start;
		int ox,oy,x,y,xsize,ysize,zoomx,zoomy,flipx,flipy,color;

		if (!(swapWord(spram[attr_start + 2]) & 0x0080)) continue;

		oy    =  swapWord(spram[attr_start + 0]) & 0x01ff;
		zoomy = (swapWord(spram[attr_start + 0]) & 0xf000) >> 12;
		ox    =  swapWord(spram[attr_start + 1]) & 0x01ff;
		zoomx = (swapWord(spram[attr_start + 1]) & 0xf000) >> 12;
		xsize = (swapWord(spram[attr_start + 2]) & 0x0700) >> 8;
		flipx =  swapWord(spram[attr_start + 2]) & 0x0800;
		ysize = (swapWord(spram[attr_start + 2]) & 0x7000) >> 12;
		flipy =  swapWord(spram[attr_start + 2]) & 0x8000;
		color = (swapWord(spram[attr_start + 2]) & 0x000f) | colo;
		map_start = swapWord(spram[attr_start + 3]);

		zoomx = 16 - zoomtable[zoomx]/8;
		zoomy = 16 - zoomtable[zoomy]/8;

		for (y = 0;y <= ysize;y++)
		{
			int sx,sy;

			if (flipy) sy = ((oy + zoomy * (ysize - y) + 16) & 0x1ff) - 16;
			else sy = ((oy + zoomy * y + 16) & 0x1ff) - 16;

			for (x = 0;x <= xsize;x++)
			{
				int code;

				if (flipx) sx = ((ox + zoomx * (xsize - x) + 16) & 0x1ff) - 16;
				else sx = ((ox + zoomx * x + 16) & 0x1ff) - 16;

				code = swapWord(spram2[map_start & 0x1fff]) & 0x1fff;
				map_start++;

				draw_16x16_zoom(gfx, code, color, sx, sy-8, zoomx, zoomy, flipx, flipy);
			}

			if (xsize == 2) map_start += 1;
			if (xsize == 4) map_start += 3;
			if (xsize == 5) map_start += 2;
			if (xsize == 6) map_start += 1;
		}
	}
}

static void f1gp2_draw_sprites()
{
	int offs = 0;
	unsigned short *spram  = (unsigned short*)DrvSprVRAM1;
	unsigned short *spram2 = (unsigned short*)DrvSprCGRAM1;

	while (offs < 0x0400 && (spram[offs] & 0x4000) == 0)
	{
		int attr_start;
		int map_start;
		int ox,oy,x,y,xsize,ysize,zoomx,zoomy,flipx,flipy,color;

		attr_start = 4 * (swapWord(spram[offs++]) & 0x01ff);

		oy    =  swapWord(spram[attr_start + 0]) & 0x01ff;
		ysize = (swapWord(spram[attr_start + 0]) & 0x0e00) >> 9;
		zoomy = (swapWord(spram[attr_start + 0]) & 0xf000) >> 12;
		ox    =  swapWord(spram[attr_start + 1]) & 0x01ff;
		xsize = (swapWord(spram[attr_start + 1]) & 0x0e00) >> 9;
		zoomx = (swapWord(spram[attr_start + 1]) & 0xf000) >> 12;
		flipx =  swapWord(spram[attr_start + 2]) & 0x4000;
		flipy =  swapWord(spram[attr_start + 2]) & 0x8000;
		color = (swapWord(spram[attr_start + 2]) & 0x1f00) >> 8;
		map_start = swapWord(spram[attr_start + 3]) & 0x7fff;

		zoomx = 32 - zoomx;
		zoomy = 32 - zoomy;

		if (swapWord(spram[attr_start + 2]) & 0x20ff) color = GetCurrentFrame() & 0x0f; // good enough?

		for (y = 0;y <= ysize;y++)
		{
			int sx,sy;

			if (flipy) sy = ((oy + zoomy * (ysize - y)/2 + 16) & 0x1ff) - 16;
			else sy = ((oy + zoomy * y / 2 + 16) & 0x1ff) - 16;

			for (x = 0;x <= xsize;x++)
			{
				int code;

				if (flipx) sx = ((ox + zoomx * (xsize - x) / 2 + 16) & 0x1ff) - 16;
				else sx = ((ox + zoomx * x / 2 + 16) & 0x1ff) - 16;

				code = swapWord(spram2[map_start & 0x3fff]) & 0x3fff;
				map_start++;

				if (*flipscreen) {
					flipx = !flipx;
					flipy = !flipy;
					sx = 304-sx;
					sy = 208-sy;
				}

				draw_16x16_zoom(DrvGfxROM1, code, color | 0x20, sx, sy, 16, 16, flipx, flipy);
			}
		}
	}
}

static void predraw_roz_tile(int offset)
{
	unsigned short data = swapWord(*((unsigned short*)(DrvRozVidRAM + offset)));

	offset >>= 1;

	int sx = (offset & 0x3f) << 4;
	int sy = (offset >> 6) << 4;

	unsigned short *dst = DrvBgTmp + (sy * 1024) + sx;

	int code = (data & 0x7ff) | (*roz_bank << 11);
	int color = ((data >> 12) << 4) | ((nScreenStartY) ? 0x300 : 0x100);
	unsigned char *src = DrvGfxROM3 + (code << 8);

	for (int y = 0; y < 16; y++) {
		for (int x = 0; x < 16; x++) {
			dst[x] = src[x] | color;
			if (src[x] == 0x0f) dst[x] |= 0x8000;
		}
		dst += 1024;
		src += 16;
	}
}

static void predraw_background()
{
	unsigned short *ptr = (unsigned short*)DrvRozVidRAM;
	for (int i = 0; i < 0x2000; i+=2, ptr++) {
		if (DrvBgDirty[i/2] || DrvBgTileDirty[swapWord(*ptr) & 0x7ff]) {
			predraw_roz_tile(i);
			DrvBgDirty[i/2] = 0;
		}
	}

	memset (DrvBgTileDirty, 0, 0x800);
}

static inline void copy_roz(unsigned int startx, unsigned int starty, int incxx, int incxy, int incyx, int incyy, int transp)
{
	unsigned short *dst = pTransDraw;
	unsigned short *src = DrvBgTmp;

	for (int sy = 0; sy < nScreenHeight; sy++, startx+=incyx, starty+=incyy)
	{
		unsigned int cx = startx;
		unsigned int cy = starty;

		if (transp) {
			for (int x = 0; x < nScreenWidth; x++, cx+=incxx, cy+=incxy, dst++)
			{
				int pxl = (src[(((cy >> 16) & 0x3ff) << 10) + ((cx >> 16) & 0x3ff)]);

				if (!(pxl & 0x8000)) {
					*dst = (pxl);
				}
			}
		} else {
			for (int x = 0; x < nScreenWidth; x++, cx+=incxx, cy+=incxy, dst++) {
				*dst = (src[(((cy >> 16) & 0x3ff) << 10) + ((cx >> 16) & 0x3ff)]) & 0x3ff;
			}
		}
	}
}

static void draw_background(int transp)
{
	unsigned short *ctrl = DrvBgCtrl;

	unsigned int startx,starty;
	int incxx,incxy,incyx,incyy;

	startx = 256 * (short)(swapWord(ctrl[0x00]));
	starty = 256 * (short)(swapWord(ctrl[0x01]));
	incyx  =       (short)(swapWord(ctrl[0x02]));
	incyy  =       (short)(swapWord(ctrl[0x03]));
	incxx  =       (short)(swapWord(ctrl[0x04]));
	incxy  =       (short)(swapWord(ctrl[0x05]));

	if (swapWord(ctrl[0x06]) & 0x4000) { incyx *= 256; incyy *= 256; }
	if (swapWord(ctrl[0x06]) & 0x0040) { incxx *= 256; incxy *= 256; }

	if (nScreenStartY) {
		startx -= -10 * incyx;
		starty -= -10 * incyy;
		startx -= -58 * incxx;
		starty -= -58 * incxy;
	} else {
		startx -= -21 * incyx;
		starty -= -21 * incyy;
		startx -= -48 * incxx;
		starty -= -48 * incxy;
	}

	copy_roz(startx << 5, starty << 5, incxx << 5, incxy << 5, incyx << 5, incyy << 5, transp);
}

static void recalculate_palette()
{
	if (DrvRecalc) {
		unsigned char r,g,b;
		unsigned short *p = (unsigned short*)DrvPalRAM;
		for (int i = 0; i < 0x400; i++) {
			unsigned short pbe = swapWord(p[i]);
			r = (pbe >> 10) & 0x1f;
			g = (pbe >>  5) & 0x1f;
			b = (pbe >>  0) & 0x1f;

			r = (r << 3) | (r >> 2);
			g = (g << 3) | (g >> 2);
			b = (b << 3) | (b >> 2);

			DrvPalette[i] = BurnHighCol(r, g, b, 0);
		}
		DrvPalette[0x400] = 0;
	}
}

static int F1gpDraw()
{
	recalculate_palette();
	predraw_background();
	draw_background(0);

	if (*gfxctrl == 0x00) {
		if (nBurnLayer & 2) f1gp_draw_sprites(DrvSprVRAM2, DrvSprCGRAM2, DrvGfxROM2, 0x20);
		if (nBurnLayer & 1) f1gp_draw_sprites(DrvSprVRAM1, DrvSprCGRAM1, DrvGfxROM1, 0x10);
		if (nBurnLayer & 4) draw_foreground(1);
	} else {
		if (nBurnLayer & 2) f1gp_draw_sprites(DrvSprVRAM2, DrvSprCGRAM2, DrvGfxROM2, 0x20);
		if (nBurnLayer & 4) draw_foreground(1);
		if (nBurnLayer & 1) f1gp_draw_sprites(DrvSprVRAM1, DrvSprCGRAM1, DrvGfxROM1, 0x10);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static int F1gp2Draw()
{
	recalculate_palette();
	predraw_background();

	if (!(*gfxctrl & 4)) {
		switch (*gfxctrl & 3)
		{
			case 0:
				if (nBurnLayer & 1) draw_background(0);
				if (nBurnLayer & 4) f1gp2_draw_sprites();
				if (nBurnLayer & 2) draw_foreground(1);
			break;

			case 1:
				if (nBurnLayer & 1) draw_background(0);
				if (nBurnLayer & 2) draw_foreground(1);
				if (nBurnLayer & 4) f1gp2_draw_sprites();
			break;

			case 2:
				if (nBurnLayer & 2) draw_foreground(0);
				if (nBurnLayer & 1) draw_background(1);
				if (nBurnLayer & 4) f1gp2_draw_sprites();
			break;
		}
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static int F1gpbDraw()
{
	recalculate_palette();
	BurnTransferClear();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static int DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		DrvInputs[0] = 0xffff;
		for (int i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

	}

	int nInterleave = 100;
	int nCyclesTotal[3] = { 10000000 / 60, 10000000 / 60, 5000000 / 60 };
	int nCyclesDone[3] = { 0, 0, 0 };

	ZetOpen(0);

	for (int i = 0; i < nInterleave; i++) {
		int nCycleSegment = ((nCyclesTotal[0] / nInterleave) * (i+1)) - nCyclesDone[0];

		SekOpen(0);
		nCyclesDone[0] += SekRun(nCycleSegment);
		if (i == (nInterleave - 1)) SekSetIRQLine(1, SEK_IRQSTATUS_AUTO);
		SekClose();

		SekOpen(1);
		nCyclesDone[1] += SekRun(nCycleSegment);
		if (i == (nInterleave - 1)) SekSetIRQLine(1, SEK_IRQSTATUS_AUTO);
		SekClose();
	}

	BurnTimerEndFrame(nCyclesTotal[2] - ZetTotalCycles());

	if (pBurnSoundOut) {
		BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static int DrvScan(int nAction, int *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2610Scan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		if (nScreenStartY) {
			for (int i = 0; i < 0x40000; i+=2) {
				expand_dynamic_tiles(i);
			}
		}

		ZetOpen(0);
		sound_bankswitch(*DrvZ80Bank);
		ZetClose();
	}

	return 0;
}


// F-1 Grand Prix

static struct BurnRomInfo f1gpRomDesc[] = {
	{ "rom1-a.3",	0x020000, 0x2d8f785b, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "rom11-a.2",	0x040000, 0x53df8ea1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom10-a.1",	0x040000, 0x46a289fb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom13-a.4",	0x040000, 0x7d92e1fa, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rom12-a.3",	0x040000, 0xd8c1bcf4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "rom6-a.6",	0x040000, 0x6d947a3f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "rom7-a.5",	0x040000, 0x7a014ba6, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "rom9-a.8",	0x040000, 0x49286572, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "rom8-a.7",	0x040000, 0x0ed783c7, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "rom2-a.06",	0x100000, 0x747dd112, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "rom3-a.05",	0x100000, 0x264aed13, 1 | BRF_PRG | BRF_ESS }, // 10

	{ "rom4-a.4",	0x020000, 0x8e811d36, 2 | BRF_PRG | BRF_ESS }, // 11 68K #1 Code

	{ "rom5-a.8",	0x020000, 0x9ea36e35, 3 | BRF_PRG | BRF_ESS }, // 12 Z80 Code

	{ "rom3-b.07",	0x100000, 0xffb1d489, 4 | BRF_GRA }, 	       // 13 Character Tiles
	{ "rom2-b.04",	0x100000, 0xd1b3471f, 4 | BRF_GRA }, 	       // 14

	{ "rom5-b.2",	0x080000, 0x17572b36, 5 | BRF_GRA }, 	       // 15 Sprite Bank 0
	{ "rom4-b.3",	0x080000, 0x72d12129, 5 | BRF_GRA }, 	       // 16

	{ "rom7-b.17",	0x040000, 0x2aed9003, 6 | BRF_GRA }, 	       // 17 Sprite Bank 1
	{ "rom6-b.16",	0x040000, 0x6789ef12, 6 | BRF_GRA }, 	       // 18

	{ "rom14-a.09",	0x100000, 0xb4c1ac31, 7 | BRF_SND }, 	       // 19 YM2610 Samples
	{ "rom17-a.08",	0x100000, 0xea70303d, 7 | BRF_SND }, 	       // 20
};

STD_ROM_PICK(f1gp)
STD_ROM_FN(f1gp)

static int F1gpInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvF1gp = {
	"f1gp", NULL, NULL, "1991",
	"F-1 Grand Prix\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_MISC_MISC,
	NULL, f1gpRomInfo, f1gpRomName, F1gpInputInfo, F1gpDIPInfo,
	F1gpInit, DrvExit, DrvFrame, F1gpDraw, DrvScan, &DrvRecalc,
	240, 320, 3, 4
};


// F-1 Grand Prix (Playmark bootleg)

static struct BurnRomInfo f1gpbRomDesc[] = {
	{ "1.ic38",	0x020000, 0x046dd83a, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 code
	{ "7.ic39",	0x020000, 0x960f5db4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2.ic48",	0x080000, 0xb3b315c3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "8.ic41",	0x080000, 0x39af8180, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "3.ic165",	0x080000, 0xb7295a30, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "9.ic166",	0x080000, 0xbb596d5b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "4.ic42",	0x080000, 0x5dbde98a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "10.ic43",	0x080000, 0xd60e7706, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "5.ic167",	0x080000, 0x48c36293, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "11.ic168",	0x080000, 0x92a28e52, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "16.u7",	0x010000, 0x7609d818, 2 | BRF_PRG | BRF_ESS }, // 10 68K #1 Code
	{ "17.u6",	0x010000, 0x951befde, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "13.ic151",	0x080000, 0x4238074b, 3 | BRF_GRA }, 	       // 12 Character Tiles
	{ "12.ic152",	0x080000, 0xe97c2b6e, 3 | BRF_GRA }, 	       // 13
	{ "15.ic153",	0x080000, 0xc2867d7f, 3 | BRF_GRA }, 	       // 14
	{ "14.ic154",	0x080000, 0x0cd20423, 3 | BRF_GRA }, 	       // 15

	{ "rom21",	0x080000, 0x7a08c3b7, 4 | BRF_GRA }, 	       // 16 Sprite Banks
	{ "rom20",	0x080000, 0xbd1273d0, 4 | BRF_GRA }, 	       // 17
	{ "19.ic141",	0x080000, 0xaa4ebdfe, 4 | BRF_GRA }, 	       // 18
	{ "18.ic140",	0x080000, 0x9b2a4325, 4 | BRF_GRA }, 	       // 19

	{ "6.ic13",	0x080000, 0x6e83ffd8, 5 | BRF_SND }, 	       // 20 OKI MSM6295 Samples
};

STD_ROM_PICK(f1gpb)
STD_ROM_FN(f1gpb)

static int F1gpbInit()
{
	return 1;
}

struct BurnDriverD BurnDrvF1gpb = {
	"f1gpb", "f1gp", NULL, "1991",
	"F-1 Grand Prix (Playmark bootleg)\0", NULL, "[Video System Co.] (Playmark bootleg)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG, 1, HARDWARE_MISC_MISC,
	NULL, f1gpbRomInfo, f1gpbRomName, F1gpInputInfo, F1gpDIPInfo,
	F1gpbInit, DrvExit, DrvFrame, F1gpbDraw, DrvScan, &DrvRecalc,
	240, 320, 3, 4
};


// F-1 Grand Prix Part II

static struct BurnRomInfo f1gp2RomDesc[] = {
	{ "rom12.v1",	0x020000, 0xc5c5f199, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "rom14.v2",	0x020000, 0xdd5388e2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom2",	0x200000, 0x3b0cfa82, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "rom13.v3",	0x020000, 0xc37aa303, 2 | BRF_PRG | BRF_ESS }, //  3 68K #1 Code

	{ "rom5.v4",	0x020000, 0x6a9398a1, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rom1",	0x200000, 0xf2d55ad7, 4 | BRF_GRA }, 	       //  5 Character Tiles

	{ "rom15",	0x200000, 0x1ac03e2e, 5 | BRF_GRA }, 	       //  6 Sprites

	{ "rom11",	0x100000, 0xb22a2c1f, 6 | BRF_GRA }, 	       //  7 Background Tiles
	{ "rom10",	0x100000, 0x43fcbe23, 6 | BRF_GRA }, 	       //  8
	{ "rom9",	0x100000, 0x1bede8a1, 6 | BRF_GRA }, 	       //  9
	{ "rom8",	0x100000, 0x98baf2a1, 6 | BRF_GRA }, 	       // 10

	{ "rom4",	0x080000, 0xc2d3d7ad, 7 | BRF_SND }, 	       // 11 YM2610 Samples
	{ "rom3",	0x100000, 0x7f8f066f, 7 | BRF_SND }, 	       // 12
};

STD_ROM_PICK(f1gp2)
STD_ROM_FN(f1gp2)

static int F1gp2Init()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvF1gp2 = {
	"f1gp2", NULL, NULL, "1992",
	"F-1 Grand Prix Part II\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_MISC_MISC,
	NULL, f1gp2RomInfo, f1gp2RomName, F1gp2InputInfo, F1gp2DIPInfo,
	F1gp2Init, DrvExit, DrvFrame, F1gp2Draw, DrvScan, &DrvRecalc,
	224, 320, 3, 4
};

