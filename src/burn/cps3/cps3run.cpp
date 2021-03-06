/**********************************************************************

CPS3 Driver (preliminary)

Decryption by Andreas Naive

Driver by David Haywood
 with help from Tomasz Slanina and ElSemi

Sound emulation by Philip Bennett

SCSI code by ElSemi

**********************************************************************

Port to FBA by OopsWare

**********************************************************************/

#include "burnint.h"
#include "sh2.h"
#ifdef SN_TARGET_PS3
#include "highcol.h"
#endif

#define BE_GFX		1 // Big-endian?!

#define FAST_BOOT	1
#define SPEED_HACK	1 // Default should be 1, if not FPS would drop.

#define CPS3_VOICES		16

#define CPS3_SND_INT_RATE	(nBurnFPS / 100)
#define CPS3_SND_RATE		(42954500 / 3 / 384)
#define CPS3_SND_BUFFER_SIZE	(CPS3_SND_RATE / CPS3_SND_INT_RATE)
#define CPS3_SND_LINEAR_SHIFT	12

static uint8_t *Mem = NULL;
static uint8_t *MemEnd = NULL;
static uint8_t *RamStart;
static uint8_t *RamEnd;

static uint8_t *RomBios;
static uint8_t *RomGame;
static uint8_t *RomGame_D;
static uint8_t *RomUser;

static uint8_t *RamMain;
static uint32_t *RamSpr;
static uint16_t *RamPal;

static uint32_t *RamCRam;

static uint32_t *RamSS;

static uint32_t *RamVReg;

static uint8_t *RamC000;
static uint8_t *RamC000_D;

static uint16_t *EEPROM;

static uint16_t *CurPal;
static uint32_t *RamScreen;

static uint8_t cps3_reset = 0;
static uint8_t cps3_palette_change = 0;

static uint32_t cps3_key1, cps3_key2, cps3_isSpecial, cps3_redearthHack;
static uint32_t cps3_bios_test_hack, cps3_game_test_hack;
static uint32_t cps3_speedup_ram_address, cps3_speedup_code_address;
static uint8_t cps3_dip;
static uint32_t cps3_region_address, cps3_ncd_address;

static uint32_t cps3_data_rom_size;

static uint8_t Cps3But1[16];
static uint8_t Cps3But2[16];
static uint8_t Cps3But3[16];

static uint16_t Cps3Input[4] = {0, 0, 0, 0};

static uint32_t ss_bank_base = 0;
static uint32_t ss_pal_base = 0;

static uint32_t cram_bank = 0;
static uint16_t cps3_current_eeprom_read = 0;
static uint32_t gfxflash_bank = 0;

static uint32_t paldma_source = 0;
static uint32_t paldma_dest = 0;
static uint32_t paldma_fade = 0;
static uint32_t paldma_length = 0;

static uint32_t chardma_source = 0;
static uint32_t chardma_table_address = 0;

static int cps3_gfx_width, cps3_gfx_height;
static int cps3_gfx_max_x, cps3_gfx_max_y;


// -- AMD/Fujitsu 29F016 --------------------------------------------------

enum {
	FM_NORMAL,	// normal read/write
	FM_READID,	// read ID
	FM_READSTATUS,	// read status
	FM_WRITEPART1,	// first half of programming, awaiting second
	FM_CLEARPART1,	// first half of clear, awaiting second
	FM_SETMASTER,	// first half of set master lock, awaiting on/off
	FM_READAMDID1,	// part 1 of alt ID sequence
	FM_READAMDID2,	// part 2 of alt ID sequence
	FM_READAMDID3,	// part 3 of alt ID sequence
	FM_ERASEAMD1,	// part 1 of AMD erase sequence
	FM_ERASEAMD2,	// part 2 of AMD erase sequence
	FM_ERASEAMD3,	// part 3 of AMD erase sequence
	FM_ERASEAMD4,	// part 4 of AMD erase sequence
	FM_BYTEPROGRAM
};

typedef struct
{
	int status;
	int flash_mode;
	int flash_master_lock;
} flash_chip;

static flash_chip main_flash;

void cps3_flash_init(flash_chip * chip/*, void *data*/)
{
	memset(chip, 0, sizeof(flash_chip));
	chip->status = 0x80;
	chip->flash_mode = FM_NORMAL;
	chip->flash_master_lock = 0;

}typedef struct {
	unsigned short regs[16];
	unsigned int pos;
	unsigned short frac;
} cps3_voice;

typedef struct {
	cps3_voice voice[CPS3_VOICES];
	unsigned short key;

	unsigned char * rombase;
	unsigned int delta;

} cps3snd_chip;

static cps3snd_chip * chip;

#define cps3SndReset()
#define cps3SndExit() free(chip);

static uint32_t cps3_flash_read(flash_chip * chip, uint32_t addr)
{
	switch( chip->flash_mode )
	{
		case FM_READAMDID3:
		case FM_READID:

			switch (addr & 0x7fffff)
			{
				case 0:
					return 0x04040404;	//(c->maker_id << 0) | (c->maker_id << 8) | (c->maker_id << 16) | (c->maker_id << 24);
				case 4:
					return 0xadadadad;	//(c->device_id << 0) | (c->device_id << 8) | (c->device_id << 16) | (c->device_id << 24);
				case 8:
					return 0x00000000;
					//case 12: return (c->flash_master_lock) ? 0x01010101 : 0x00000000
			}
			return 0;

		case FM_ERASEAMD4:
			chip->status ^= ( 1 << 6 ) | ( 1 << 2 );
		case FM_READSTATUS:
			return (chip->status << 0) | (chip->status << 8) | (chip->status << 16) | (chip->status << 24);
		case FM_NORMAL:
		default:
			//
			return 0;
	}
}

static void cps3_flash_write(flash_chip * chip, uint32_t addr, uint32_t data)
{
	//bprintf(1, _T("FLASH to write long value %8x to location %8x\n"), data, addr);

	switch( chip->flash_mode )
	{
		case FM_NORMAL:
		case FM_READSTATUS:
		case FM_READID:
		case FM_READAMDID3:
			switch( data & 0xff )
			{
				case 0xf0:
				case 0xff: chip->flash_mode = FM_NORMAL;	break;
				case 0x90: chip->flash_mode = FM_READID;	break;
				case 0x40:
				case 0x10: chip->flash_mode = FM_WRITEPART1;break;
				case 0x50:	// clear status reg
					   chip->status = 0x80;
					   chip->flash_mode = FM_READSTATUS;
					   break;
				case 0x20: chip->flash_mode = FM_CLEARPART1;break;
				case 0x60: chip->flash_mode = FM_SETMASTER;	break;
				case 0x70: chip->flash_mode = FM_READSTATUS;break;
				case 0xaa:	// AMD ID select part 1
					   if ((addr & 0xffff) == (0x555 << 2))
						   chip->flash_mode = FM_READAMDID1;
					   break;
					   //default:
					   //logerror( "Unknown flash mode byte %x\n", data & 0xff );
					   //bprintf(1, _T("FLASH to write long value %8x to location %8x\n"), data, addr);
					   //break;
			}
			break;
		case FM_READAMDID1:
			if ((addr & 0xffff) == (0x2aa <<2) && (data & 0xff) == 0x55 )
				chip->flash_mode = FM_READAMDID2;
			else
			{
				//logerror( "unexpected %08x=%02x in FM_READAMDID1\n", address, data & 0xff );
				chip->flash_mode = FM_NORMAL;
			}
			break;
		case FM_READAMDID2:
			if ((addr & 0xffff) == (0x555<<2) && (data & 0xff) == 0x90)
				chip->flash_mode = FM_READAMDID3;
			else if((addr & 0xffff) == (0x555<<2) && (data & 0xff) == 0x80)
				chip->flash_mode = FM_ERASEAMD1;
			else if((addr & 0xffff) == (0x555<<2) && (data & 0xff) == 0xa0)
				chip->flash_mode = FM_BYTEPROGRAM;
			else
			{
				// logerror( "unexpected %08x=%02x in FM_READAMDID2\n", address, data & 0xff );
				chip->flash_mode = FM_NORMAL;
			}
			break;
	}
}

// ------------------------------------------------------------------------

static uint16_t rotate_left(uint16_t value, int n)
{
	int aux = value>>(16-n);
	return ((value<<n)|aux) & 0xffff;
}

static uint16_t rotxor(uint16_t val, uint16_t x)
{
	uint16_t res;
	res = val + rotate_left(val,2);
	res = rotate_left(res,4) ^ (res & (val ^ x));
	return res;
}

static uint32_t cps3_mask(uint32_t address, uint32_t key1, uint32_t key2)
{
	uint16_t val;
	address ^= key1;
	val = (address & 0xffff) ^ 0xffff;
	val = rotxor(val, key2 & 0xffff);
	val ^= (address >> 16) ^ 0xffff;
	val = rotxor(val, key2 >> 16);
	val ^= (address & 0xffff) ^ (key2 & 0xffff);
	return val | (val << 16);
}

static void cps3_decrypt_bios(void)
{
	uint32_t * coderegion = (uint32_t *)RomBios;
	for (int i=0; i<0x20000; i+=4)
	{
		uint32_t xormask = cps3_mask(i, cps3_key1, cps3_key2);
		/* a bit of a hack, don't decrypt the FLASH commands which are transfered by SH2 DMA */
		if ( (i<0x1ff00) || (i>0x1ff6b) )
			coderegion[i/4] ^= xormask;
	}
}

static void cps3_decrypt_game(void)
{
	uint32_t * coderegion = (uint32_t *)RomGame;
	uint32_t * decrypt_coderegion = (uint32_t *)RomGame_D;

	for (int i=0; i<0x1000000; i+=4) {
		uint32_t xormask = cps3_mask(i + 0x06000000, cps3_key1, cps3_key2);
		decrypt_coderegion[i/4] = coderegion[i/4] ^ xormask;
	}
}


static int last_normal_byte = 0;

static uint32_t process_byte( uint8_t real_byte, uint32_t destination, int max_length )
{
	uint8_t * dest = (uint8_t *) RamCRam;
	//printf("process byte for destination %08x\n", destination);
	destination &= 0x7fffff;

	if (real_byte&0x40) {
		int tranfercount = 0;
		//printf("Set RLE Mode\n");
		int cps3_rle_length = (real_byte&0x3f)+1;
		//printf("RLE Operation (length %08x\n", cps3_rle_length );
		while (cps3_rle_length) {
#if BE_GFX
			dest[((destination+tranfercount)&0x7fffff)] = (last_normal_byte&0x3f);
#else
			dest[((destination+tranfercount)&0x7fffff)^3] = (last_normal_byte&0x3f);
#endif

			tranfercount++;
			cps3_rle_length--;
			max_length--;
			if ((destination+tranfercount) > 0x7fffff)
				return max_length;
		}
		return tranfercount;
	} else {
		//printf("Write Normal Data\n");
#if BE_GFX
		dest[(destination&0x7fffff)] = real_byte;
#else
		dest[(destination&0x7fffff)^3] = real_byte;
#endif
		last_normal_byte = real_byte;
		return 1;
	}
}

static void cps3_do_char_dma( uint32_t real_source, uint32_t real_destination, uint32_t real_length )
{
	uint8_t * sourcedata = RomUser;
	int length_remaining = real_length;
	last_normal_byte = 0;
	while (length_remaining) {
		uint8_t current_byte = sourcedata[ real_source ^ 0 ];
		real_source++;

		if (current_byte & 0x80) {
			uint8_t real_byte;
			uint32_t length_processed;
			current_byte &= 0x7f;

			real_byte = sourcedata[ (chardma_table_address+current_byte*2+0) ^ 0 ];
			//if (real_byte&0x80) return;
			length_processed = process_byte( real_byte, real_destination, length_remaining );
			length_remaining -= length_processed; // subtract the number of bytes the operation has taken
			real_destination += length_processed; // add it onto the destination
			if (real_destination>0x7fffff || length_remaining <= 0)
				return; // if we've expired, exit

			real_byte = sourcedata[ (chardma_table_address+current_byte*2+1) ^ 0 ];
			//if (real_byte&0x80) return;
			length_processed = process_byte( real_byte, real_destination, length_remaining );
			length_remaining -= length_processed; // subtract the number of bytes the operation has taken
			real_destination += length_processed; // add it onto the destination
			if (real_destination>0x7fffff || length_remaining <= 0)
				return;	// if we've expired, exit
		} else {
			uint32_t length_processed;
			length_processed = process_byte( current_byte, real_destination, length_remaining );
			length_remaining -= length_processed; // subtract the number of bytes the operation has taken
			real_destination += length_processed; // add it onto the destination
			if (real_destination>0x7fffff || length_remaining <= 0)
				return; // if we've expired, exit
		}
	}
}

static uint16_t lastb;
static uint16_t lastb2;

static uint32_t ProcessByte8(uint8_t b, uint32_t dst_offset)
{
	uint8_t * destRAM = (uint8_t *) RamCRam;
 	int l=0;

 	if(lastb==lastb2) {	//rle
 		int rle=(b+1)&0xff;

 		for(int i=0;i<rle;++i)
		{
#if BE_GFX
			destRAM[(dst_offset&0x7fffff)] = lastb;
#else
			destRAM[(dst_offset&0x7fffff)^3] = lastb;
#endif
			//cps3_char_ram_dirty[(dst_offset&0x7fffff)/0x100] = 1;
			//cps3_char_ram_is_dirty = 1;

			dst_offset++;
			++l;
		}
 		lastb2=0xffff;
 		return l;
 	} else {
 		lastb2=lastb;
 		lastb=b;
#if BE_GFX
		destRAM[(dst_offset&0x7fffff)] = b;
#else
		destRAM[(dst_offset&0x7fffff)^3] = b;
#endif
		//cps3_char_ram_dirty[(dst_offset&0x7fffff)/0x100] = 1;
		//cps3_char_ram_is_dirty = 1;
 		return 1;
 	}
}

static void cps3_do_alt_char_dma(uint32_t src, uint32_t real_dest, uint32_t real_length )
{
	uint8_t * px = RomUser;
	uint32_t start = real_dest;
	uint32_t ds = real_dest;

	lastb=0xfffe;
	lastb2=0xffff;

	while(1) {
		uint8_t ctrl=px[ src ^ 0 ];
 		++src;

		for(int i=0;i<8;++i) {
			uint8_t p = px[ src ^ 0 ];

			if(ctrl&0x80) {
				uint8_t real_byte;
				p &= 0x7f;
				real_byte = px[ (chardma_table_address+p*2+0) ^ 0 ];
				ds += ProcessByte8(real_byte,ds);
				real_byte = px[ (chardma_table_address+p*2+1) ^ 0 ];
				ds += ProcessByte8(real_byte,ds);
 			} else {
 				ds += ProcessByte8(p,ds);
 			}
 			++src;
 			ctrl<<=1;

			if((ds-start)>=real_length)
				return;
 		}
	}
}

static void cps3_process_character_dma(uint32_t address)
{
	for (int i=0; i<0x1000; i+=3)
	{
		uint32_t dat1 = RamCRam[i+0+(address)];
		uint32_t dat2 = RamCRam[i+1+(address)];
		uint32_t dat3 = RamCRam[i+2+(address)];
		uint32_t real_source      = (dat3<<1)-0x400000;
		uint32_t real_destination =  dat2<<3;
		uint32_t real_length      = (((dat1&0x001fffff)+1)<<3);

		if (dat1==0x01000000) break;	// end of list marker
		if (dat1==0x13131313) break;	// our default fill

		switch ( dat1 & 0x00e00000 )
		{
			case 0x00800000:
				chardma_table_address = real_source;
				Sh2SetIRQLine(10, SH2_IRQSTATUS_AUTO);
				break;
			case 0x00400000:
				cps3_do_char_dma( real_source, real_destination, real_length );
				Sh2SetIRQLine(10, SH2_IRQSTATUS_AUTO);
				break;
			case 0x00600000:
				//bprintf(PRINT_NORMAL, _T("Character DMA (alt) start %08x to %08x with %d\n"), real_source, real_destination, real_length);
				/* 8bpp DMA decompression
				   - this is used on SFIII NG Sean's Stage ONLY */
				cps3_do_alt_char_dma( real_source, real_destination, real_length );
				Sh2SetIRQLine(10, SH2_IRQSTATUS_AUTO);
				break;
			case 0x00000000:
				// Red Earth need this. 8192 byte trans to 0x00003000 (from 0x007ec000???)
				// seems some stars(6bit alpha) without compress
				//bprintf(PRINT_NORMAL, _T("Character DMA (redearth) start %08x to %08x with %d\n"), real_source, real_destination, real_length);
				memcpy( (uint8_t *)RamCRam + real_destination, RomUser + real_source, real_length );
				Sh2SetIRQLine(10, SH2_IRQSTATUS_AUTO);
				break;
				//default:
				//bprintf(PRINT_NORMAL, _T("Character DMA Unknown DMA List Command Type %08x\n"), dat1);
		}
	}
}

static int MemIndex()
{
	uint8_t *Next; Next = Mem;
	RomBios 	= Next; Next += 0x0080000;

	RomUser		= Next; Next += cps3_data_rom_size;	// 0x5000000;

	RamStart	= Next;

	RomGame 	= Next; Next += 0x1000000;
	RomGame_D 	= Next; Next += 0x1000000;

	RamC000		= Next; Next += 0x0000400;
	RamC000_D	= Next; Next += 0x0000400;

	RamMain		= Next; Next += 0x0080000;

	RamPal		= (uint16_t *) Next; Next += 0x0040000;
	RamSpr		= (uint32_t *) Next; Next += 0x0080000;

	RamCRam		= (uint32_t *) Next; Next += 0x0800000;
	RamSS		= (uint32_t *) Next; Next += 0x0010000;

	RamVReg		= (uint32_t *) Next; Next += 0x0000100;

	EEPROM		= (uint16_t *) Next; Next += 0x0000400;

	RamEnd		= Next;

	CurPal		= (uint16_t *) Next; Next += 0x040000;
	RamScreen	= (uint32_t *) Next; Next += (512 * 2) * (224 * 2 + 32) * sizeof(int);

	MemEnd		= Next;
	return 0;
}

static uint8_t __fastcall cps3ReadByte(uint32_t addr)
{
	#if 0
	addr &= 0xc7ffffff;
	#endif
	return 0;
}

static uint16_t __fastcall cps3ReadWord(uint32_t addr)
{
	addr &= 0xc7ffffff;

	switch (addr)
	{
		case 0x040c0000:
		case 0x040c0002:
		case 0x040c0004:
		case 0x040c0006:
			// cps3_vbl_r
		case 0x040c000c:
		case 0x040c000e:
			return 0;
		case 0x05000000:
			return ~Cps3Input[1];
		case 0x05000002:
			return ~Cps3Input[0];
		case 0x05000004:
			return ~Cps3Input[3];
		case 0x05000006:
			return ~Cps3Input[2];
		case 0x05140000:
		case 0x05140002:
				 // cd-rom read

				 break;

		default:
				 if ((addr >= 0x05000a00) && (addr < 0x05000a20))
					 return 0xffff;	// cps3_unk_io_r
				 else if ((addr >= 0x05001000) && (addr < 0x05001204))
				 {
					 // EEPROM
					 addr -= 0x05001000;
					 if (addr >= 0x100 && addr < 0x180)
						 cps3_current_eeprom_read = EEPROM[((addr-0x100) >> 1)];
					 else
						 if (addr == 0x202)
							 return cps3_current_eeprom_read;
				 }
				//else
				//bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %8x\n"), addr);
	}
	return 0;
}

static uint32_t __fastcall cps3ReadLong(uint32_t addr)
{
	addr &= 0xc7ffffff;

	if(addr == 0x04200000)
		return 0x0404adad;

	#if 0
	switch (addr)
	{
		case 0x04200000:
			//bprintf(PRINT_NORMAL, _T("GFX Read Flash ID, cram bank %04x gfx flash bank: %04x\n"), cram_bank, gfxflash_bank);
			return 0x0404adad;
			//default:
			//bprintf(PRINT_NORMAL, _T("Attempt to read long value of location %8x\n"), addr);
	}
	#endif
	return 0;
}

static void __fastcall cps3WriteByte(uint32_t addr, uint8_t data)
{
	addr &= 0xc7ffffff;

	switch (addr)
	{
		// cps3_ss_bank_base_w
		case 0x05050020:
			ss_bank_base = ( ss_bank_base  & 0x00ffffff ) | (data << 24);
			break;
		case 0x05050021:
			ss_bank_base = ( ss_bank_base  & 0xff00ffff ) | (data << 16);
			break;
		case 0x05050022:
			ss_bank_base = ( ss_bank_base  & 0xffff00ff ) | (data <<  8);
			break;
		case 0x05050023:
			ss_bank_base = ( ss_bank_base  & 0xffffff00 ) | (data <<  0);
			break;
			// cps3_ss_pal_base_w
		case 0x05050024:
			ss_pal_base = ( ss_pal_base & 0x00ff ) | (data << 8);
			break;
		case 0x05050025:
			ss_pal_base = ( ss_pal_base & 0xff00 ) | (data << 0);
			break;
		case 0x05050026:
		case 0x05050027:
			break;

#if 0
		default:
			if ((addr >= 0x05050000) && (addr < 0x05060000))
			{
				// VideoReg

			}
			else
				bprintf(PRINT_NORMAL, _T("Attempt to write byte value   %02x to location %8x\n"), data, addr);
#endif
	}
}

static void __fastcall cps3WriteWord(uint32_t addr, uint16_t data)
{
	addr &= 0xc7ffffff;

	switch (addr)
	{

		case 0x040c0084: break;
		case 0x040c0086:
				 if (cram_bank != data)
				 {
					 cram_bank = data & 7;
					 //bprintf(PRINT_NORMAL, _T("CRAM bank set to %d\n"), data);
					 Sh2MapMemory(((uint8_t *)RamCRam) + (cram_bank << 20), 0x04100000, 0x041fffff, SM_RAM);
				 }
				 break;

		case 0x040c0088:
				 //case 0x040c008a:
				 gfxflash_bank = data - 2;
				 //bprintf(PRINT_NORMAL, _T("gfxflash bank set to %04x\n"), data);
				 break;

				 // cps3_characterdma_w
				 //case 0x040c0094:
		case 0x040c0096:
				 chardma_source = data;
				 break;
		case 0x040c0098:
				 //case 0x040c009a:
				 if (data & 0x0040)
					 cps3_process_character_dma( chardma_source | ((data & 0x003f) << 16) );
				 break;

				 // cps3_palettedma_w
		case 0x040c00a0: paldma_source = (paldma_source & 0x0000ffff) | (data << 16); break;
		case 0x040c00a2: paldma_source = (paldma_source & 0xffff0000) | (data <<  0); break;
		case 0x040c00a4: paldma_dest = (paldma_dest & 0x0000ffff) | (data << 16); break;
		case 0x040c00a6: paldma_dest = (paldma_dest & 0xffff0000) | (data <<  0); break;
		case 0x040c00a8: paldma_fade = (paldma_fade & 0x0000ffff) | (data << 16); break;
		case 0x040c00aa: paldma_fade = (paldma_fade & 0xffff0000) | (data <<  0); break;
		case 0x040c00ac: paldma_length = data; break;
		case 0x040c00ae:
				 //bprintf(PRINT_NORMAL, _T("palettedma [%04x]  from %08x to %08x fade %08x size %d\n"), data, (paldma_source << 1), paldma_dest, paldma_fade, paldma_length);
				 if (data & 0x0002) {
					 for (uint32_t i=0; i<paldma_length; i++) {
						 uint16_t * src = (uint16_t *)RomUser;
						 uint16_t coldata = src[(paldma_source - 0x200000 + i)];

#if defined LSB_FIRST
						 coldata = (coldata << 8) | (coldata >> 8);
#endif

						 uint32_t r = (coldata & 0x001F) >>  0;
						 uint32_t g = (coldata & 0x03E0) >>  5;
						 uint32_t b = (coldata & 0x7C00) >> 10;
						 if (paldma_fade!=0)
						 {
							 int fade;
							 fade = (paldma_fade & 0x3f000000)>>24; r = (r*fade)>>5; if (r>0x1f) r = 0x1f;
							 fade = (paldma_fade & 0x003f0000)>>16; g = (g*fade)>>5; if (g>0x1f) g = 0x1f;
							 fade = (paldma_fade & 0x0000003f)>> 0; b = (b*fade)>>5; if (b>0x1f) b = 0x1f;
							 coldata = (r << 0) | (g << 5) | (b << 10);
						 }

						 r = r << 3;
						 g = g << 3;
						 b = b << 3;

						 RamPal[(paldma_dest + i)] = coldata;
						 CurPal[(paldma_dest + i) ] = BurnHighCol(r, g, b, 0);
					 }
					 Sh2SetIRQLine(10, SH2_IRQSTATUS_AUTO);
				 }
				 break;

		case 0x04200554:
		case 0x04200aaa:
				 // gfx flash command
				 // "0xaa 0x55 0x90" set flash to get id
				 // "0xaa 0x55 0xf0" set flash to normal read
				 break;

				 // cps3_ss_bank_base_w
		case 0x05050020: ss_bank_base = ( ss_bank_base  & 0x0000ffff ) | (data << 16); break;
		case 0x05050022: ss_bank_base = ( ss_bank_base  & 0xffff0000 ) | (data <<  0); break;

				 // cps3_ss_pal_base_w
		case 0x05050024: ss_pal_base = data; break;
		case 0x05050026: break;

		case 0x05100000:
				 Sh2SetIRQLine(12, SH2_IRQSTATUS_NONE);
				 break;
		case 0x05110000:
				 Sh2SetIRQLine(10, SH2_IRQSTATUS_NONE);
				 break;

		case 0x05140000:
		case 0x05140002:
				 // cd-rom read

				 break;

		default:
				 if ((addr >= 0x040c0000) && (addr < 0x040c0100))
				 {
					 // 0x040C0000 ~ 0x040C001f : cps3_unk_vidregs
					 // 0x040C0020 ~ 0x040C002b : tilemap20_regs_base
					 // 0x040C0030 ~ 0x040C003b : tilemap30_regs_base
					 // 0x040C0040 ~ 0x040C004b : tilemap40_regs_base
					 // 0x040C0050 ~ 0x040C005b : tilemap50_regs_base
					 // 0x040C0060 ~ 0x040C007f : cps3_fullscreenzoom

					 addr &= 0xff;
					 ((uint16_t *)RamVReg)[ (addr >> 1) ] = data;

				 }
				 else
					 if (!((addr >= 0x05000000) && (addr < 0x05001000)))
						 if ((addr >= 0x05001000) && (addr < 0x05001204))
						 {
							 // EEPROM
							 addr -= 0x05001000;
							 if ((addr>=0x080) && (addr<0x100))
								 EEPROM[((addr-0x080) >> 1) ] = data;
						 }
#if 0
						 else
							 if ((addr >= 0x05050000) && (addr < 0x05060000))
							 {
								 // unknown i/o

							 }
#endif
				 //else
				 //bprintf(PRINT_NORMAL, _T("Attempt to write word value %04x to location %8x\n"), data, addr);
	}
}

static void __fastcall cps3WriteLong(uint32_t addr, uint32_t data)
{
	#if 0
	addr &= 0xc7ffffff;

	switch (addr)
	{
	case 0x07ff000c:
	case 0x07ff0048:
		// some unknown data write by DMA 0 while bootup
		break;
	//default:
		//bprintf(PRINT_NORMAL, _T("Attempt to write long value %8x to location %8x\n"), data, addr);
	}
	#endif
}

static void __fastcall cps3C0WriteByte(uint32_t addr, uint8_t data)
{
	//bprintf(PRINT_NORMAL, _T("C0 Attempt to write byte value %2x to location %8x\n"), data, addr);
}

static void __fastcall cps3C0WriteWord(uint32_t addr, uint16_t data)
{
	//bprintf(PRINT_NORMAL, _T("C0 Attempt to write word value %4x to location %8x\n"), data, addr);
}

static void __fastcall cps3C0WriteLong(uint32_t addr, uint32_t data)
{
	if (addr < 0xc0000400)
	{
		*(uint32_t *)(RamC000 + (addr & 0x3ff)) = data;
		*(uint32_t *)(RamC000_D + (addr & 0x3ff)) = data ^ cps3_mask(addr, cps3_key1, cps3_key2);
		return ;
	}
	//bprintf(PRINT_NORMAL, _T("C0 Attempt to write long value %8x to location %8x\n"), data, addr);
}

// If fastboot != 1

static uint8_t __fastcall cps3RomReadByte(uint32_t addr)
{
//	bprintf(PRINT_NORMAL, _T("Rom Attempt to read byte value of location %8x\n"), addr);
	addr &= 0xc7ffffff;
	//addr ^= 0x03;
/*	uint32_t pc = Sh2GetPC(0);
	if (pc == cps3_bios_test_hack || pc == cps3_game_test_hack){
		bprintf(PRINT_NORMAL, _T("CPS3 Hack : read byte from %08x\n"), addr);
		return *(RomGame + (addr & 0x00ffffff));
	}  */
	return *(RomGame_D + (addr & 0x00ffffff));
}

static uint16_t __fastcall cps3RomReadWord(uint32_t addr)
{
//	bprintf(PRINT_NORMAL, _T("Rom Attempt to read word value of location %8x\n"), addr);
	addr &= 0xc7ffffff;
//	addr ^= 0x02;
/*	uint32_t pc = Sh2GetPC(0);
	if (pc == cps3_bios_test_hack || pc == cps3_game_test_hack){
		bprintf(PRINT_NORMAL, _T("CPS3 Hack : read word from %08x\n"), addr);
		return *(uint16_t *)(RomGame + (addr & 0x00ffffff));
	} */
	return *(uint16_t *)(RomGame_D + (addr & 0x00ffffff));
}

static uint32_t __fastcall cps3RomReadLong(uint32_t addr)
{
//	bprintf(PRINT_NORMAL, _T("Rom Attempt to read long value of location %8x\n"), addr);
	addr &= 0xc7ffffff;

	uint32_t retvalue = cps3_flash_read(&main_flash, addr);
	if ( main_flash.flash_mode == FM_NORMAL )
		retvalue = *(uint32_t *)(RomGame_D + (addr & 0x00ffffff));

	uint32_t pc = Sh2GetPC(0);
	if (pc == cps3_bios_test_hack || pc == cps3_game_test_hack)
	{
		if ( main_flash.flash_mode == FM_NORMAL )
			retvalue = *(uint32_t *)(RomGame + (addr & 0x00ffffff));
		//bprintf(2, _T("CPS3 Hack : read long from %08x [%08x]\n"), addr, retvalue );
	}
	return retvalue;
}

static void __fastcall cps3RomWriteByte(uint32_t addr, uint8_t data)
{
	//bprintf(PRINT_NORMAL, _T("Rom Attempt to write byte value %2x to location %8x\n"), data, addr);
}

static void __fastcall cps3RomWriteWord(uint32_t addr, uint16_t data)
{
	//bprintf(PRINT_NORMAL, _T("Rom Attempt to write word value %4x to location %8x\n"), data, addr);
}

static void __fastcall cps3RomWriteLong(uint32_t addr, uint32_t data)
{
//	bprintf(1, _T("Rom Attempt to write long value %8x to location %8x\n"), data, addr);
	addr &= 0x00ffffff;
	cps3_flash_write(&main_flash, addr, data);

	if ( main_flash.flash_mode == FM_NORMAL )
	{
		//bprintf(1, _T("Rom Attempt to write long value %8x to location %8x\n"), data, addr);
		*(uint32_t *)(RomGame + addr) = data;
		*(uint32_t *)(RomGame_D + addr) = data ^ cps3_mask(addr + 0x06000000, cps3_key1, cps3_key2);
	}
}

static uint8_t __fastcall cps3RomReadByteSpe(uint32_t addr)
{
//	bprintf(PRINT_NORMAL, _T("Rom Attempt to read byte value of location %8x\n"), addr);
	addr &= 0xc7ffffff;
//	addr ^= 0x03;
	return *(RomGame + (addr & 0x00ffffff));
}

static uint16_t __fastcall cps3RomReadWordSpe(uint32_t addr)
{
//	bprintf(PRINT_NORMAL, _T("Rom Attempt to read word value of location %8x\n"), addr);
	addr &= 0xc7ffffff;
	//addr ^= 0x02;
	return *(uint16_t *)(RomGame + (addr & 0x00ffffff));
}

static uint32_t __fastcall cps3RomReadLongSpe(uint32_t addr)
{
//	bprintf(PRINT_NORMAL, _T("Rom Attempt to read long value of location %8x\n"), addr);
	addr &= 0xc7ffffff;

	uint32_t retvalue = cps3_flash_read(&main_flash, addr);
	if ( main_flash.flash_mode == FM_NORMAL )
		retvalue = *(uint32_t *)(RomGame + (addr & 0x00ffffff));

	return retvalue;
}

static uint8_t __fastcall cps3VidReadByte(uint32_t addr)
{
	//bprintf(PRINT_NORMAL, _T("Video Attempt to read byte value of location %8x\n"), addr);
//	addr &= 0xc7ffffff;
	return 0;
}

static uint16_t __fastcall cps3VidReadWord(uint32_t addr)
{
	//bprintf(PRINT_NORMAL, _T("Video Attempt to read word value of location %8x\n"), addr);
//	addr &= 0xc7ffffff;
	return 0;
}

static uint32_t __fastcall cps3VidReadLong(uint32_t addr)
{
	//bprintf(PRINT_NORMAL, _T("Video Attempt to read long value of location %8x\n"), addr);
//	addr &= 0xc7ffffff;
	return 0;
}

static void __fastcall cps3VidWriteByte(uint32_t addr, uint8_t data)
{
	//bprintf(PRINT_NORMAL, _T("Video Attempt to write byte value %2x to location %8x\n"), data, addr);
}

static void __fastcall cps3VidWriteWord(uint32_t addr, uint16_t data)
{
	addr &= 0xc7ffffff;
	if ((addr >= 0x04080000) && (addr < 0x040c0000))
	{
		// Palette
		uint32_t palindex = (addr - 0x04080000) >> 1;
		RamPal[palindex] = data;

		int r = (data & 0x001F) << 3;	// Red
		int g = (data & 0x03E0) >> 2;	// Green
		int b = (data & 0x7C00) >> 7;	// Blue

		r |= r >> 5;
		g |= g >> 5;
		b |= b >> 5;

		CurPal[palindex] = BurnHighCol(r, g, b, 0);

	}
}

static void __fastcall cps3VidWriteLong(uint32_t addr, uint32_t data)
{
#if 0
	addr &= 0xc7ffffff;

	if ((addr >= 0x04080000) && (addr < 0x040c0000))
	{
		if ( data != 0 )
			bprintf(PRINT_NORMAL, _T("Video Attempt to write long value %8x to location %8x\n"), data, addr);

	}
	else
		bprintf(PRINT_NORMAL, _T("Video Attempt to write long value %8x to location %8x\n"), data, addr);
#endif
}


static uint8_t __fastcall cps3RamReadByte(uint32_t addr)
{
	if (addr == cps3_speedup_ram_address )
		if (Sh2GetPC(0) == cps3_speedup_code_address)
			Sh2BurnUntilInt(0);

	addr &= 0x7ffff;
	return *(RamMain + (addr ));
}

static uint16_t __fastcall cps3RamReadWord(uint32_t addr)
{
	//bprintf(PRINT_NORMAL, _T("Ram Attempt to read long value of location %8x\n"), addr);
	addr &= 0x7ffff;

	if (addr == cps3_speedup_ram_address )
		if (Sh2GetPC(0) == cps3_speedup_code_address)
		{
			//bprintf(PRINT_NORMAL, _T("Ram Attempt to read long value of location %8x\n"), addr);
			Sh2BurnUntilInt(0);
		}

	return *(uint16_t *)(RamMain + (addr ));
}


static uint32_t __fastcall cps3RamReadLong(uint32_t addr)
{
	if (addr == cps3_speedup_ram_address )
		if (Sh2GetPC(0) == cps3_speedup_code_address)
			Sh2BurnUntilInt(0);

	addr &= 0x7ffff;
	return *(uint32_t *)(RamMain + addr);
}

// CPS3 Region Patching for regular games
static void Cps3PatchRegion()
{
	if ( cps3_region_address )
	{

//		bprintf(0, _T("%02x  %02x\n"), RomBios[cps3_region_address], RomBios[cps3_ncd_address]);
		RomBios[cps3_region_address ^ 0x03] = (RomBios[cps3_region_address ^ 0x03] & 0xf0) | (cps3_dip & 0xff);

		if ( cps3_ncd_address )
		{
			if (cps3_dip & 0x10)
				RomBios[cps3_ncd_address] |= 0x01;
			else
				RomBios[cps3_ncd_address] &= 0xfe;
		}
	}
}

static int Cps3Reset()
{
	// re-map cram_bank
	cram_bank = 0;
	Sh2MapMemory((uint8_t *)RamCRam, 0x04100000, 0x041fffff, SM_RAM);

	Cps3PatchRegion();

	// [CD-ROM not emulated] All CHD drivers cause a Guru Meditation with the normal bios boot.
	if (!(BurnDrvGetHardwareCode() & HARDWARE_CAPCOM_CPS3_NO_CD)) {
		// normal boot
		Sh2Reset( *(uint32_t *)(RomBios + 0), *(uint32_t *)(RomBios + 4) );
	} else {
		// fast boot
		if (cps3_isSpecial) {
			Sh2Reset( *(uint32_t *)(RomGame + 0), *(uint32_t *)(RomGame + 4) );
			Sh2SetVBR(0x06000000);
		} else {
			Sh2Reset( *(uint32_t *)(RomGame_D + 0), *(uint32_t *)(RomGame_D + 4) );
			Sh2SetVBR(0x06000000);
		}
	}

	if (cps3_dip & 0x80) {
		EEPROM[0x11] = 0x100 + (EEPROM[0x11] & 0xff);
		EEPROM[0x29] = 0x100 + (EEPROM[0x29] & 0xff);
	} else {
		EEPROM[0x11] = 0x000 + (EEPROM[0x11] & 0xff);
		EEPROM[0x29] = 0x000 + (EEPROM[0x29] & 0xff);
	}

	cps3_current_eeprom_read = 0;
	cps3SndReset();
	cps3_reset = 0;

	BurnAfterReset();
	return 0;
}

#ifdef LSB_FIRST
static void cps3_be_to_le(uint8_t * p, int size)
{
	uint8_t c;
	for(uint32_t i = 0; i < size; i+=4, p+=4)
	{
		c = *(p+0);	*(p+0) = *(p+3);	*(p+3) = c;
		c = *(p+1);	*(p+1) = *(p+2);	*(p+2) = c;
	}
}
#endif

static unsigned char __fastcall cps3SndReadByte(unsigned int addr)
{
	//addr &= 0x000003ff;
	//bprintf(PRINT_NORMAL, _T("SND Attempt to read byte value of location %8x\n"), addr);
	return 0;
}

static unsigned short __fastcall cps3SndReadWord(unsigned int addr)
{
	addr &= 0x000003ff;
	
	if (addr < 0x200)
		return chip->voice[addr >> 5].regs[(addr>>1) & 0xf];
	else if (addr == 0x200)
		return chip->key;
	#if 0
	else
	bprintf(PRINT_NORMAL, _T("SND Attempt to read word value of location %8x\n"), addr);
	#endif
	return 0;
}

static unsigned int __fastcall cps3SndReadLong(unsigned int addr)
{
	//addr &= 0x000003ff;
	//bprintf(PRINT_NORMAL, _T("SND Attempt to read long value of location %8x\n"), addr);
	return 0;
}

static void __fastcall cps3SndWriteByte(unsigned int addr, unsigned char data)
{
	//addr &= 0x000003ff;
	//bprintf(PRINT_NORMAL, _T("SND Attempt to write byte value %2x to location %8x\n"), data, addr);
}

static void __fastcall cps3SndWriteWord(unsigned int addr, unsigned short data)
{
	addr &= 0x000003ff;
	
	if (addr < 0x200) {
		chip->voice[addr >> 5].regs[(addr>>1) & 0xf] = data;
		//bprintf(PRINT_NORMAL, _T("SND Attempt to write word value %4x to Chip[%02d][%02d] %s\n"), data, addr >> 5, (addr>>2) & 7, (addr & 0x02) ? "lo" : "hi" );
	} else if (addr == 0x200)
	{
		unsigned short key = data;
		for (int i = 0; i < CPS3_VOICES; i++)
		{
			// Key off -> Key on
			if ((key & (1 << i)) && !(chip->key & (1 << i)))
			{
				chip->voice[i].frac = 0;
				chip->voice[i].pos = 0;
			}
		}
		chip->key = key;
	}
	#if 0
	else
		bprintf(PRINT_NORMAL, _T("SND Attempt to write word value %4x to location %8x\n"), data, addr);
	#endif
	
}

static void __fastcall cps3SndWriteLong(unsigned int addr, unsigned int data)
{
	//addr &= 0x000003ff;
	//bprintf(PRINT_NORMAL, _T("SND Attempt to write long value %8x to location %8x\n"), data, addr);
}

static int cps3SndInit(unsigned char * sndrom)
{
	chip = (cps3snd_chip *) malloc( sizeof(cps3snd_chip) );
	if ( chip ) {
		memset( chip, 0, sizeof(cps3snd_chip) );
		chip->rombase = sndrom;
		
		/* 
		 * CPS-3 Sound chip clock: 42954500 / 3 / 384 = 37286.89
		 * Sound interupt 80Hz 
		 */
		
		if (nBurnSoundRate) {
			//chip->delta = 37286.9 / nBurnSoundRate;
			chip->delta = (CPS3_SND_BUFFER_SIZE << CPS3_SND_LINEAR_SHIFT) / nBurnSoundLen;
			//bprintf(0, _T("BurnSnd %08x, %d, %d\n"), chip->delta, chip->burnlen, nBurnSoundLen);
		}
		
		return 0;
	}
	return 1;
}

static int cps3Init()
{
	int nRet, ii, offset;
	struct BurnRomInfo pri;

	// calc graphic and sound roms size
	ii = 0; cps3_data_rom_size = 0;
	while (BurnDrvGetRomInfo(&pri, ii) == 0)
	{
		if (pri.nType & (BRF_GRA | BRF_SND))
			cps3_data_rom_size += pri.nLen;
		ii++;
	}

	// CHD games
	if (cps3_data_rom_size == 0)
		cps3_data_rom_size = 0x5000000;

	Mem = NULL;
	MemIndex();
	int nLen = (intptr_t)MemEnd;
	if ((Mem = (uint8_t *)malloc(nLen)) == NULL)
		return 1;
	memset(Mem, 0, nLen);	// blank all memory
	MemIndex();

	// load and decode bios roms
	ii = 0; offset = 0;
	while (BurnDrvGetRomInfo(&pri, ii) == 0)
	{
		if (pri.nType & BRF_BIOS)
		{
			nRet = BurnLoadRom(RomBios + offset, ii, 1);
			if (nRet != 0)
				return 1;
			offset += pri.nLen;
		}
		ii++;
	}

#ifdef LSB_FIRST
	cps3_be_to_le( RomBios, 0x080000 );	 
#endif
	cps3_decrypt_bios();

	// load and decode sh-2 program roms
	ii = 0;	offset = 0;
	while (BurnDrvGetRomInfo(&pri, ii) == 0) {
		if (pri.nType & BRF_PRG) {
			nRet = BurnLoadRom(RomGame + offset, ii, 1);
			if (nRet != 0)
				return 1;
			offset += pri.nLen;
		}
		ii++;
	}

#ifdef LSB_FIRST
	cps3_be_to_le( RomGame, 0x1000000 );
#endif
	cps3_decrypt_game();

	// load graphic and sound roms
	ii = 0;	offset = 0;
	while (BurnDrvGetRomInfo(&pri, ii) == 0)
	{
		if (pri.nType & (BRF_GRA | BRF_SND))
		{
			BurnLoadRom(RomUser + offset, ii, 1);
			offset += pri.nLen;
		}
		ii++;
	}

	Sh2Init(1);
	Sh2Open(0);

	// Map 68000 memory:
	Sh2MapMemory(RomBios, 0x00000000, 0x0007ffff, SM_ROM);				// BIOS
	Sh2MapMemory(RamMain, 0x02000000, 0x0207ffff, SM_RAM);				// Main RAM
	Sh2MapMemory((uint8_t *) RamSpr, 0x04000000, 0x0407ffff, SM_RAM);
	//Sh2MapMemory(RamCRam,		0x04100000, 0x041fffff, SM_RAM);		// map this while reset
	//Sh2MapMemory(RamGfx,		0x04200000, 0x043fffff, SM_WRITE);
	Sh2MapMemory((uint8_t *) RamSS, 0x05040000, 0x0504ffff, SM_RAM);		// 'SS' RAM (Score Screen) (text tilemap + toles)

	Sh2SetReadByteHandler (0, cps3ReadByte);
	Sh2SetReadWordHandler (0, cps3ReadWord);
	Sh2SetReadLongHandler (0, cps3ReadLong);
	Sh2SetWriteByteHandler(0, cps3WriteByte);
	Sh2SetWriteWordHandler(0, cps3WriteWord);
	Sh2SetWriteLongHandler(0, cps3WriteLong);

	Sh2MapMemory(RamC000_D,	0xc0000000, 0xc00003ff, SM_FETCH);			// Executes code from here
	Sh2MapMemory(RamC000, 0xc0000000, 0xc00003ff, SM_READ);
	Sh2MapHandler(1, 0xc0000000, 0xc00003ff, SM_WRITE);

	Sh2SetWriteByteHandler(1, cps3C0WriteByte);
	Sh2SetWriteWordHandler(1, cps3C0WriteWord);
	Sh2SetWriteLongHandler(1, cps3C0WriteLong);

	if (!(BurnDrvGetHardwareCode() & HARDWARE_CAPCOM_CPS3_NO_CD)) {
		if (cps3_isSpecial) {
			Sh2MapMemory(RomGame,	0x06000000, 0x06ffffff, SM_READ);	// Decrypted SH2 Code
			Sh2MapMemory(RomGame_D,	0x06000000, 0x06ffffff, SM_FETCH);	// Decrypted SH2 Code
		} else {
			Sh2MapMemory(RomGame_D,	0x06000000, 0x06ffffff, SM_READ | SM_FETCH);	// Decrypted SH2 Code
		}
	} else {
		Sh2MapMemory(RomGame_D,	0x06000000, 0x06ffffff, SM_FETCH);		// Decrypted SH2 Code
		Sh2MapHandler(2, 0x06000000, 0x06ffffff, SM_READ | SM_WRITE);

		if (cps3_isSpecial) {
			Sh2SetReadByteHandler (2, cps3RomReadByteSpe);
			Sh2SetReadWordHandler (2, cps3RomReadWordSpe);
			Sh2SetReadLongHandler (2, cps3RomReadLongSpe);
			Sh2SetWriteByteHandler(2, cps3RomWriteByte);
			Sh2SetWriteWordHandler(2, cps3RomWriteWord);
			Sh2SetWriteLongHandler(2, cps3RomWriteLong);
		} else {
			Sh2SetReadByteHandler (2, cps3RomReadByte);
			Sh2SetReadWordHandler (2, cps3RomReadWord);
			Sh2SetReadLongHandler (2, cps3RomReadLong);
			Sh2SetWriteByteHandler(2, cps3RomWriteByte);
			Sh2SetWriteWordHandler(2, cps3RomWriteWord);
			Sh2SetWriteLongHandler(2, cps3RomWriteLong);
		}
	}

	Sh2MapHandler(3, 0x040e0000, 0x040e02ff, SM_RAM);
	Sh2SetReadByteHandler (3, cps3SndReadByte);
	Sh2SetReadWordHandler (3, cps3SndReadWord);
	Sh2SetReadLongHandler (3, cps3SndReadLong);
	Sh2SetWriteByteHandler(3, cps3SndWriteByte);
	Sh2SetWriteWordHandler(3, cps3SndWriteWord);
	Sh2SetWriteLongHandler(3, cps3SndWriteLong);

	Sh2MapMemory((uint8_t *)RamPal, 0x04080000, 0x040bffff, SM_READ);		// 16bit BE Colors
	Sh2MapHandler(4, 0x04080000, 0x040bffff, SM_WRITE);

	Sh2SetReadByteHandler (4, cps3VidReadByte);
	Sh2SetReadWordHandler (4, cps3VidReadWord);
	Sh2SetReadLongHandler (4, cps3VidReadLong);
	Sh2SetWriteByteHandler(4, cps3VidWriteByte);
	Sh2SetWriteWordHandler(4, cps3VidWriteWord);
	Sh2SetWriteLongHandler(4, cps3VidWriteLong);

#ifdef SPEED_HACK
	// install speedup read handler
	Sh2MapHandler(5, 0x02000000 | (cps3_speedup_ram_address & 0x030000),
			0x0200ffff | (cps3_speedup_ram_address & 0x030000), SM_READ);
	Sh2SetReadByteHandler (5, cps3RamReadByte);
	Sh2SetReadWordHandler (5, cps3RamReadWord);
	Sh2SetReadLongHandler (5, cps3RamReadLong);
#endif

	BurnDrvGetVisibleSize(&cps3_gfx_width, &cps3_gfx_height);
	RamScreen += (512 * 2) * 16 + 16; // safe draw
	cps3SndInit(RomUser);
	Cps3Reset();
	return 0;
}

static int cps3Exit()
{
	int Width, Height;
	BurnDrvGetVisibleSize(&Width, &Height);

	if (Width != 384) {
		BurnDrvSetVisibleSize(384, 224);
		BurnDrvSetAspect(4, 3);
	}

	Sh2Exit();

	free(Mem);
	Mem = NULL;

	cps3SndExit();

	return 0;
}


static void cps3_drawgfxzoom_0(uint32_t code, uint32_t pal, int flipx, int flipy, int x, int y)
{
	if ((x > (cps3_gfx_width - 8)) || (y > (cps3_gfx_height - 8)))
		return;

	uint16_t * dst = (uint16_t *) pBurnDraw;
	uint8_t * src = (uint8_t *)RamSS;
	uint16_t * color = CurPal + (pal << 4);
	dst += (y * cps3_gfx_width + x);
	src += code * 64;

	int add_width = cps3_gfx_width;
	if (flipy)
	{
		add_width = -cps3_gfx_width;
		dst += cps3_gfx_width * 7;
	}

	if (flipx)
		for(int i = 0; i < 8; i++, dst += cps3_gfx_width, src += 8)
		{
			if ( src[ 1] & 0xf ) dst[7] = color [ src[ 1] & 0xf ];
			if ( src[ 1] >>  4 ) dst[6] = color [ src[ 1] >>  4 ];
			if ( src[ 3] & 0xf ) dst[5] = color [ src[ 3] & 0xf ];
			if ( src[ 3] >>  4 ) dst[4] = color [ src[ 3] >>  4 ];
			if ( src[ 5] & 0xf ) dst[3] = color [ src[ 5] & 0xf ];
			if ( src[ 5] >>  4 ) dst[2] = color [ src[ 5] >>  4 ];
			if ( src[ 7] & 0xf ) dst[1] = color [ src[ 7] & 0xf ];
			if ( src[ 7] >>  4 ) dst[0] = color [ src[ 7] >>  4 ];
		}
	else
		for(int i = 0; i < 8; i++, dst += cps3_gfx_width, src += 8)
		{
			if ( src[ 1] & 0xf ) dst[0] = color [ src[ 1] & 0xf ];
			if ( src[ 1] >>  4 ) dst[1] = color [ src[ 1] >>  4 ];
			if ( src[ 3] & 0xf ) dst[2] = color [ src[ 3] & 0xf ];
			if ( src[ 3] >>  4 ) dst[3] = color [ src[ 3] >>  4 ];
			if ( src[ 5] & 0xf ) dst[4] = color [ src[ 5] & 0xf ];
			if ( src[ 5] >>  4 ) dst[5] = color [ src[ 5] >>  4 ];
			if ( src[ 7] & 0xf ) dst[6] = color [ src[ 7] & 0xf ];
			if ( src[ 7] >>  4 ) dst[7] = color [ src[ 7] >>  4 ];
		}

}

static void cps3_drawgfxzoom_1(uint32_t code, uint32_t pal, int flipx, int flipy, int x, int y, int drawline)
{
	uint32_t * dst = RamScreen;
	uint8_t * src = (uint8_t *) RamCRam;
	dst += (drawline * 1024 + x);

#if BE_GFX

	if (flipy)
		src += code * 256 + 16 * (15 - (drawline - y));
	else
		src += code * 256 + 16 * (drawline - y);

	if ( flipx )
	{
		if ( src[ 0] ) dst[15] = src[ 0] | pal;
		if ( src[ 1] ) dst[14] = src[ 1] | pal;
		if ( src[ 2] ) dst[13] = src[ 2] | pal;
		if ( src[ 3] ) dst[12] = src[ 3] | pal;
		if ( src[ 4] ) dst[11] = src[ 4] | pal;
		if ( src[ 5] ) dst[10] = src[ 5] | pal;
		if ( src[ 6] ) dst[ 9] = src[ 6] | pal;
		if ( src[ 7] ) dst[ 8] = src[ 7] | pal;
		if ( src[ 8] ) dst[ 7] = src[ 8] | pal;
		if ( src[ 9] ) dst[ 6] = src[ 9] | pal;
		if ( src[10] ) dst[ 5] = src[10] | pal;
		if ( src[11] ) dst[ 4] = src[11] | pal;
		if ( src[12] ) dst[ 3] = src[12] | pal;
		if ( src[13] ) dst[ 2] = src[13] | pal;
		if ( src[14] ) dst[ 1] = src[14] | pal;
		if ( src[15] ) dst[ 0] = src[15] | pal;
	}
	else
	{
		if ( src[ 0] ) dst[ 0] = src[ 0] | pal;
		if ( src[ 1] ) dst[ 1] = src[ 1] | pal;
		if ( src[ 2] ) dst[ 2] = src[ 2] | pal;
		if ( src[ 3] ) dst[ 3] = src[ 3] | pal;
		if ( src[ 4] ) dst[ 4] = src[ 4] | pal;
		if ( src[ 5] ) dst[ 5] = src[ 5] | pal;
		if ( src[ 6] ) dst[ 6] = src[ 6] | pal;
		if ( src[ 7] ) dst[ 7] = src[ 7] | pal;
		if ( src[ 8] ) dst[ 8] = src[ 8] | pal;
		if ( src[ 9] ) dst[ 9] = src[ 9] | pal;
		if ( src[10] ) dst[10] = src[10] | pal;
		if ( src[11] ) dst[11] = src[11] | pal;
		if ( src[12] ) dst[12] = src[12] | pal;
		if ( src[13] ) dst[13] = src[13] | pal;
		if ( src[14] ) dst[14] = src[14] | pal;
		if ( src[15] ) dst[15] = src[15] | pal;
	}
#else

	if (flipy)
		src += code * 256 + 16 * (15 - (drawline - y));
	else
		src += code * 256 + 16 * (drawline - y);

	if (flipx)
	{
		if ( src[ 3] ) dst[15] = src[ 3] | pal;
		if ( src[ 2] ) dst[14] = src[ 2] | pal;
		if ( src[ 1] ) dst[13] = src[ 1] | pal;
		if ( src[ 0] ) dst[12] = src[ 0] | pal;
		if ( src[ 7] ) dst[11] = src[ 7] | pal;
		if ( src[ 6] ) dst[10] = src[ 6] | pal;
		if ( src[ 5] ) dst[ 9] = src[ 5] | pal;
		if ( src[ 4] ) dst[ 8] = src[ 4] | pal;
		if ( src[11] ) dst[ 7] = src[11] | pal;
		if ( src[10] ) dst[ 6] = src[10] | pal;
		if ( src[ 9] ) dst[ 5] = src[ 9] | pal;
		if ( src[ 8] ) dst[ 4] = src[ 8] | pal;
		if ( src[15] ) dst[ 3] = src[15] | pal;
		if ( src[14] ) dst[ 2] = src[14] | pal;
		if ( src[13] ) dst[ 1] = src[13] | pal;
		if ( src[12] ) dst[ 0] = src[12] | pal;
	}
	else
	{
		if ( src[ 3] ) dst[ 0] = src[ 3] | pal;
		if ( src[ 2] ) dst[ 1] = src[ 2] | pal;
		if ( src[ 1] ) dst[ 2] = src[ 1] | pal;
		if ( src[ 0] ) dst[ 3] = src[ 0] | pal;
		if ( src[ 7] ) dst[ 4] = src[ 7] | pal;
		if ( src[ 6] ) dst[ 5] = src[ 6] | pal;
		if ( src[ 5] ) dst[ 6] = src[ 5] | pal;
		if ( src[ 4] ) dst[ 7] = src[ 4] | pal;
		if ( src[11] ) dst[ 8] = src[11] | pal;
		if ( src[10] ) dst[ 9] = src[10] | pal;
		if ( src[ 9] ) dst[10] = src[ 9] | pal;
		if ( src[ 8] ) dst[11] = src[ 8] | pal;
		if ( src[15] ) dst[12] = src[15] | pal;
		if ( src[14] ) dst[13] = src[14] | pal;
		if ( src[13] ) dst[14] = src[13] | pal;
		if ( src[12] ) dst[15] = src[12] | pal;
	}
#endif
}

static void cps3_drawgfxzoom_2(uint32_t code, uint32_t pal, int flipx, int flipy, int sx, int sy, int scalex, int scaley, int alpha)
{
	//if (!scalex || !scaley) return;

	uint8_t * source_base = (uint8_t *) RamCRam + code * 256;

	int sprite_screen_height = (scaley * 16 + 0x8000) >> 16;
	int sprite_screen_width  = (scalex * 16 + 0x8000) >> 16;
	if (sprite_screen_width && sprite_screen_height)
	{
		// compute sprite increment per screen pixel
		int dx = 1048576 / sprite_screen_width;
		int dy = 1048576 / sprite_screen_height;

		int ex = sx + sprite_screen_width;
		int ey = sy + sprite_screen_height;

		int x_index_base;
		int y_index;

		if( flipx )
		{
			x_index_base = (sprite_screen_width - 1) * dx;
			dx = -dx;
		}
		else
			x_index_base = 0;

		if(flipy)
		{
			y_index = (sprite_screen_height - 1) * dy;
			dy = -dy;
		}
		else
			y_index = 0;

		if( sx < 0)
		{ /* clip left */
			int pixels = 0-sx;
			sx += pixels;
			x_index_base += pixels*dx;
		}
		if( sy < 0 )
		{ /* clip top */
			int pixels = 0-sy;
			sy += pixels;
			y_index += pixels*dy;
		}
		if( ex > cps3_gfx_max_x+1 )
		{ /* clip right */
			int pixels = ex-cps3_gfx_max_x-1;
			ex -= pixels;
		}
		if( ey > cps3_gfx_max_y+1 )
		{ /* clip bottom */
			int pixels = ey-cps3_gfx_max_y-1;
			ey -= pixels;
		}

		if( ex > sx )
		{
			switch( alpha )
			{
				case 0:
					for( int y=sy; y<ey; y++ )
					{
						uint8_t * source = source_base + (y_index>>16) * 16;
						uint32_t * dest = RamScreen + y * 512 * 2;
						int x_index = x_index_base;
						for(int x=sx; x<ex; x++ )
						{
#if BE_GFX
							uint8_t c = source[ (x_index>>16) ];
#else
							uint8_t c = source[ (x_index>>16) ^ 3 ];
#endif
							if( c )	dest[x] = pal | c;
							x_index += dx;
						}
						y_index += dy;
					}
					break;
				case 6:
					for( int y=sy; y<ey; y++ ) {
						uint8_t * source = source_base + (y_index>>16) * 16;
						uint32_t * dest = RamScreen + y * 512 * 2;
						int x_index = x_index_base;
						for(int x=sx; x<ex; x++ ) {
#if BE_GFX
							uint8_t c = source[ (x_index>>16)];
#else
							uint8_t c = source[ (x_index>>16) ^ 3 ];
#endif
							dest[x] |= ((c&0x0000f) << 13);
							x_index += dx;
						}
						y_index += dy;
					}
					break;
				case 8:
					for( int y=sy; y<ey; y++ ) {
						uint8_t * source = source_base + (y_index>>16) * 16;
						uint32_t * dest = RamScreen + y * 512 * 2;
						int x_index = x_index_base;
						for(int x=sx; x<ex; x++ ) {
#if BE_GFX
							uint8_t c = source[ (x_index>>16) ];
#else
							uint8_t c = source[ (x_index>>16) ^ 3 ];
#endif

							/*
							 * 0x00000 ~ 0x07fff : normal color for sprites
							 * 0x08000 ~ 0x0ffff : alpha color for sprites's shadow or other effect
							 *
							 * 0x10000 ~ 0x17fff : normal color for sprites ?? (redearth)
							 * 0x18000 ~ 0x1ffff : fire alpha ?? (redearth)
							 * 0x17e00 ~ 0x17fff : foreground ?? (jojo)
							 */
							if (c) {
								// jojo intro

								// global   local    bground  alpha
								// -------- -------- -------- -----
								// 0x12400  0x12700  0x00500  10	: title in intro (jojo)
								// 0x00000  0x0008f  0x06a40  10	: avatar shadow in intro (jojo)
								// 0x00100  0x0008f  0x06a40  10	: star in avatar shadow in intro (jojo)

								// 0x00000  0x00000  0x01180  10	: charactor select background mask (sfiii3)
								// 0x00000  0x00000  0x0????  10	: charactor shadow (sfiii3)

								// 0x00000  0x11d00  0x01640  01	: magic transview (redearth)

								dest[x] |= 0x8000;

								// this bit seems correct to magic effect for redearth
								// but got jojo's title to black in it's intro
								// is here difference between global palette and local palette ??
								if (pal&0x10000) dest[x] |= 0x10000;

								// jojo intro , a alpha effect star in avatar's shadow
								// this bit seems to disable alpha effect
								//if (pal & 0x100) dest[x] &= 0x17fff;


							}
							x_index += dx;
						}
						y_index += dy;
					}
					break;
			}

		}
	}
}

static void cps3_draw_tilemapsprite_line(int drawline, uint32_t * regs )
{
	int scrolly =  ((regs[0]&0x0000ffff)>>0)+4;
	int line = drawline + scrolly;
	line &= 0x3ff;

	if (!(regs[1]&0x00008000))
		return;

	uint32_t mapbase =  (regs[2]&0x007f0000)>>16;
	uint32_t linebase=  (regs[2]&0x7f000000)>>24;
	int linescroll_enable = (regs[1]&0x00004000);

	int scrollx;
	int tileline = (line/16)+1;
	int tilesubline = line % 16;

	//rectangle clip;

	mapbase = mapbase << 10;
	linebase = linebase << 10;

	scrollx =  (regs[0]&0xffff0000)>>16;

	if (linescroll_enable)
		scrollx+= (RamSpr[linebase+((line+16-4)&0x3ff)]>>16)&0x3ff;

	if (drawline>cps3_gfx_max_y+4)
		return;

	//clip.min_x = cliprect->min_x;
	//clip.max_x = cliprect->max_x;
	//clip.min_y = drawline;
	//clip.max_y = drawline;

	for (int x=0;x<(cps3_gfx_max_x/16)+2;x++)
	{
		uint32_t dat;
		int tileno;
		int colour;
		int bpp;
		int xflip,yflip;

		dat = RamSpr[mapbase+((tileline&63)*64)+((x+scrollx/16)&63)];
		tileno = (dat & 0xffff0000)>>17;
		colour = (dat & 0x000001ff)>>0;
		bpp = (dat & 0x0000200)>>9;
		yflip  = (dat & 0x00000800)>>11;
		xflip  = (dat & 0x00001000)>>12;

		if (!bpp)
			colour <<= 8;
		else
			colour <<= 6;

		cps3_drawgfxzoom_1(tileno,colour,xflip,yflip,(x*16)-scrollx%16,drawline-tilesubline, drawline);
	}
}

static void DrvDraw_sfiii2()
{
	int bg_drawn[4] = { 0, 0, 0, 0 };

	// registers are normally 002a006f 01ef01c6
	//      widescreen mode = 00230076 026501c6
	// only SFIII2 uses widescreen, I don't know exactly which register controls it
	//if (((RamVReg[ 6 * 4 + 1 ]&0xffff0000)>>16) != 0x01ef) {
	//	bprintf(0, _T("Wide Screen Mode %08x\n"), RamVReg[ 6 * 4 + 1 ]);
	//}

	// fullscreenzoom 0x40 for normal size
	//                0x80 for double size
	//				  0x20 for half size
	uint32_t fullscreenzoom = RamVReg[ 6 * 4 + 3 ] & 0xff;	// cps3_fullscreenzoom[3]
	uint32_t fullscreenzoomwidecheck = RamVReg[6 * 4 + 1];

	if (fullscreenzoom > 0x80)
		fullscreenzoom = 0x80;
	uint32_t fsz = (fullscreenzoom << (16 - 6));

	cps3_gfx_max_x = ((cps3_gfx_width * fsz)  >> 16) - 1;	// 384 ( 496 for SFIII2 Only)
	cps3_gfx_max_y = ((cps3_gfx_height * fsz) >> 16) - 1;	// 224


#ifndef NO_LAYER_ENABLE_TOGGLE
	if (nBurnLayer & 1) // layer disable
	{
#endif
		// Clear Screen Buffer
		memset(RamScreen, 0, 512 * 448 * sizeof(int));
#ifndef NO_LAYER_ENABLE_TOGGLE
	}
#endif

	// Draw Sprites
	for (int i= 0; i < 2048; i += 4)
	{
		int xpos =		(RamSpr[i+1]&0x03ff0000)>>16;
		int ypos =		(RamSpr[i+1]&0x000003ff)>>0;

		int gscroll =		(RamSpr[i+0]&0x70000000)>>28;
		int length =		(RamSpr[i+0]&0x01ff0000)>>14; // how many entries in the sprite table
		uint32_t start =	(RamSpr[i+0]&0x00007ff0)>>4;

		int whichbpp =		(RamSpr[i+2]&0x40000000)>>30; // not 100% sure if this is right, jojo title / characters
		int whichpal =		(RamSpr[i+2]&0x20000000)>>29;
		int global_xflip =	(RamSpr[i+2]&0x10000000)>>28;
		int global_yflip =	(RamSpr[i+2]&0x08000000)>>27;
		int global_alpha =	(RamSpr[i+2]&0x04000000)>>26; // alpha / shadow? set on sfiii2 shadows, and big black image in jojo intro
		int global_bpp =	(RamSpr[i+2]&0x02000000)>>25;
		int global_pal =	(RamSpr[i+2]&0x01ff0000)>>16;

		int gscrollx =		(RamVReg[gscroll]&0x03ff0000)>>16;
		int gscrolly =		(RamVReg[gscroll]&0x000003ff)>>0;

		start = (start * 0x100) >> 2;

		if ((RamSpr[i+0]&0xf0000000) == 0x80000000)
			break;

		for (int j=0; j<length; j+=4)
		{
			uint32_t value1 = (RamSpr[start+j+0]);
			uint32_t value2 = (RamSpr[start+j+1]);
			uint32_t value3 = (RamSpr[start+j+2]);
			uint32_t tileno = (value1&0xfffe0000)>>17;
			int count;
			int xpos2 = (value2 & 0x03ff0000)>>16;
			int ypos2 = (value2 & 0x000003ff)>>0;
			int flipx = (value1 & 0x00001000)>>12;
			int flipy = (value1 & 0x00000800)>>11;
			int alpha = (value1 & 0x00000400)>>10; //? this one is used for alpha effects on warzard
			int bpp =   (value1 & 0x00000200)>>9;
			int pal =   (value1 & 0x000001ff);

			/* these are the sizes to actually draw */
			int ysizedraw2 = ((value3 & 0x7f000000)>>24);
			int xsizedraw2 = ((value3 & 0x007f0000)>>16);
			int xx,yy;

			int tilestable[4] = { 8,1,2,4 };
			int ysize2 = ((value3 & 0x0000000c)>>2);
			int xsize2 = ((value3 & 0x00000003)>>0);
			uint32_t xinc,yinc;

			// invalid sprite ysize of 0 tiles
			if (ysize2==0)
				continue;

			// xsize of 0 tiles seems to be a special command to draw tilemaps
			if (xsize2==0)
			{
#ifndef NO_LAYER_ENABLE_TOGGLE
				if (nBurnLayer & 1) // layer disable
				{
#endif
					int tilemapnum = ((value3 & 0x00000030)>>4);
					int startline;// = value2 & 0x3ff;
					int endline;
					int height = (value3 & 0x7f000000)>>24;
					uint32_t * regs;
					regs = RamVReg + 8 + tilemapnum * 4;
					endline = value2;
					startline = endline - height;

					startline &=0x3ff;
					endline &=0x3ff;

					// Urgh, the startline / endline seem to be direct screen co-ordinates regardless of fullscreen zoom
					// which probably means the fullscreen zoom is applied when rendering everything, not aftewards

					//for (uu=startline;uu<endline+1;uu++)

					if (bg_drawn[tilemapnum]==0)
						for (int uu=0;uu<1023;uu++)
							cps3_draw_tilemapsprite_line( uu, regs );
					bg_drawn[tilemapnum] = 1;
#ifndef NO_LAYER_ENABLE_TOGGLE
				}
#endif

			}
			else
			{
#ifndef NO_SPRITE_ENABLE_TOGGLE
				if (~nSpriteEnable & 1)
					continue;
#endif

				ysize2 = tilestable[ysize2];
				xsize2 = tilestable[xsize2];

				xinc = ((xsizedraw2+1)<<16) / ((xsize2*0x10));
				yinc = ((ysizedraw2+1)<<16) / ((ysize2*0x10));
				xsize2-=1;
				ysize2-=1;

				flipx ^= global_xflip;
				flipy ^= global_yflip;

				if (!flipx)
					xpos2+=((xsizedraw2+1)/2);
				else
					xpos2-=((xsizedraw2+1)/2);

				ypos2+=((ysizedraw2+1)/2);

				if (!flipx)
					xpos2-= (((xsize2+1)*16*xinc)>>16);
				else
					xpos2+= (((xsize2)*16*xinc)>>16);

				if (flipy)
					ypos2-= ((ysize2*16*yinc)>>16);

				count = 0;
				for (xx=0;xx<xsize2+1;xx++)
				{
					int current_xpos;

					if(!flipx)
						current_xpos = (xpos+xpos2+((xx*16*xinc)>>16));
					else
						current_xpos = (xpos+xpos2-((xx*16*xinc)>>16));
					current_xpos += gscrollx;
					current_xpos += 1;
					current_xpos &=0x3ff;
					if (current_xpos&0x200) current_xpos-=0x400;

					for (yy=0;yy<ysize2+1;yy++)
					{
						int current_ypos;
						int actualpal;
						if (flipy)
							current_ypos = (ypos+ypos2+((yy*16*yinc)>>16));
						else
							current_ypos = (ypos+ypos2-((yy*16*yinc)>>16));

						current_ypos += gscrolly;
						current_ypos = 0x3ff-current_ypos;
						current_ypos -= 17;
						current_ypos &=0x3ff;

						if (current_ypos&0x200)
							current_ypos-=0x400;

						/* use the palette value from the main list or the sublists? */
						if (whichpal)
							actualpal = global_pal;
						else
							actualpal = pal;

						/* use the bpp value from the main list or the sublists? */
						int color_granularity;
						if (whichbpp)
						{
							if (!global_bpp)
								color_granularity = 8;
							else
								color_granularity = 6;
						}
						else
						{
							if (!bpp)
								color_granularity = 8;
							else
								color_granularity = 6;
						}
						actualpal <<= color_granularity;

						int realtileno = tileno+count;

						if ( realtileno )
						{
							if (global_alpha || alpha)
							{

								// fix jojo's title in it's intro ???
								if ( global_alpha && (global_pal & 0x100))
									actualpal &= 0x0ffff;

								cps3_drawgfxzoom_2(realtileno,actualpal,0^flipx,0^flipy,current_xpos,current_ypos,xinc,yinc, color_granularity);

							}
							else
								cps3_drawgfxzoom_2(realtileno,actualpal,0^flipx,0^flipy,current_xpos,current_ypos,xinc,yinc, 0);
						}
						count++;
					}
				}
			}
		}
	}

	// copy screen from pBurnDraw with zoom
	{
		uint32_t srcx, srcy = 0;
		uint32_t * srcbitmap;
		uint16_t * dstbitmap = (uint16_t * )pBurnDraw;

		for (int rendery=0; rendery<224; rendery++) {
			srcbitmap = RamScreen + (srcy >> 16) * 512 * 2;
			srcx=0;
			for (int renderx=0; renderx<cps3_gfx_width; renderx++, dstbitmap ++)
			{
				*dstbitmap = CurPal[ srcbitmap[srcx>>16] ];
				srcx += fsz;
			}
			srcy += fsz;
		}
	}

	// fg layer
#ifndef NO_LAYER_ENABLE_TOGGLE
	if (nBurnLayer & 2) // layer disable
	{
#endif
		// bank select? (sfiii2 intro)
		int count = (ss_bank_base & 0x01000000) ? 0x0000 : 0x0800;
		for (int y=0; y<32; y++) {
			for (int x=0; x<64; x++) {
				uint32_t data = (RamSS[count]); // +0x800 = 2nd bank, used on sfiii2 intro..
				uint32_t tile = (data >> 16) & 0x1ff;
				int pal = (data & 0x003f) >> 1;
				int flipx = (data & 0x0080) >> 7;
				int flipy = (data & 0x0040) >> 6;
				pal += ss_pal_base << 5;
				tile+=0x200;
				cps3_drawgfxzoom_0(tile,pal,flipx,flipy,x<<3,y<<3);
				count++;
			}
		}
#ifndef NO_LAYER_ENABLE_TOGGLE
	}
#endif

	// switch wide screen
	if (((fullscreenzoomwidecheck & 0xffff0000) >> 16) == 0x0265)
	{
		int Width, Height;
		BurnDrvGetVisibleSize(&Width, &Height);

		if (Width != 496)
		{
			BurnDrvSetVisibleSize(496, 224);
			BurnDrvSetAspect(16, 9);
			if (BurnReinitScrn)
				BurnReinitScrn();
			BurnDrvGetVisibleSize(&cps3_gfx_width, &cps3_gfx_height);
		}
	} else {
		int Width, Height;
		BurnDrvGetVisibleSize(&Width, &Height);

		if (Width != 384) {
			BurnDrvSetVisibleSize(384, 224);
			BurnDrvSetAspect(4, 3);
			if (BurnReinitScrn)
				BurnReinitScrn();
			BurnDrvGetVisibleSize(&cps3_gfx_width, &cps3_gfx_height);
		}
	}
}

static void DrvDraw()
{
	int bg_drawn[4] = { 0, 0, 0, 0 };

	// registers are normally 002a006f 01ef01c6
	//      widescreen mode = 00230076 026501c6
	// only SFIII2 uses widescreen, I don't know exactly which register controls it
	//if (((RamVReg[ 6 * 4 + 1 ]&0xffff0000)>>16) != 0x01ef) {
	//	bprintf(0, _T("Wide Screen Mode %08x\n"), RamVReg[ 6 * 4 + 1 ]);
	//}

	// fullscreenzoom	0x40 for normal size
	//			0x80 for double size
	//			0x20 for half size
	uint32_t fullscreenzoom = RamVReg[ 6 * 4 + 3 ] & 0xff;	// cps3_fullscreenzoom[3]

	if (fullscreenzoom > 0x80)
		fullscreenzoom = 0x80;

	uint32_t fsz = (fullscreenzoom << (16 - 6));

	cps3_gfx_max_x = ((cps3_gfx_width * fsz)  >> 16) - 1;	// 384 ( 496 for SFIII2 Only)
	cps3_gfx_max_y = ((cps3_gfx_height * fsz) >> 16) - 1;	// 224

#ifndef NO_LAYER_ENABLE_TOGGLE
	if (nBurnLayer & 1) // layer disable
	{
#endif
		// Clear Screen Buffer
		memset(RamScreen, 0, 512 * 448 * sizeof(int));
#ifndef NO_LAYER_ENABLE_TOGGLE
	}
#endif

	// Draw Sprites
	for (int i = 0; i < 2048; i += 4)
	{
		int xpos =		(RamSpr[i+1]&0x03ff0000)>>16;
		int ypos =		(RamSpr[i+1]&0x000003ff)>>0;

		int gscroll =		(RamSpr[i+0]&0x70000000)>>28;
		int length =		(RamSpr[i+0]&0x01ff0000)>>14; // how many entries in the sprite table
		uint32_t start =	(RamSpr[i+0]&0x00007ff0)>>4;

		int whichbpp =		(RamSpr[i+2]&0x40000000)>>30; // not 100% sure if this is right, jojo title / characters
		int whichpal =		(RamSpr[i+2]&0x20000000)>>29;
		int global_xflip =	(RamSpr[i+2]&0x10000000)>>28;
		int global_yflip =	(RamSpr[i+2]&0x08000000)>>27;
		int global_alpha =	(RamSpr[i+2]&0x04000000)>>26; // alpha / shadow? set on sfiii2 shadows, and big black image in jojo intro
		int global_bpp =	(RamSpr[i+2]&0x02000000)>>25;
		int global_pal =	(RamSpr[i+2]&0x01ff0000)>>16;

		int gscrollx =		(RamVReg[gscroll]&0x03ff0000)>>16;
		int gscrolly =		(RamVReg[gscroll]&0x000003ff)>>0;

		start = (start * 0x100) >> 2;

		if ((RamSpr[i+0]&0xf0000000) == 0x80000000)
			break;

		for (int j=0; j<length; j+=4)
		{
			uint32_t value1 = (RamSpr[start+j+0]);
			uint32_t value2 = (RamSpr[start+j+1]);
			uint32_t value3 = (RamSpr[start+j+2]);
			uint32_t tileno = (value1&0xfffe0000)>>17;
			int count;
			int xpos2 = (value2 & 0x03ff0000)>>16;
			int ypos2 = (value2 & 0x000003ff)>>0;
			int flipx = (value1 & 0x00001000)>>12;
			int flipy = (value1 & 0x00000800)>>11;
			int alpha = (value1 & 0x00000400)>>10; //? this one is used for alpha effects on warzard
			int bpp =   (value1 & 0x00000200)>>9;
			int pal =   (value1 & 0x000001ff);

			/* these are the sizes to actually draw */
			int ysizedraw2 = ((value3 & 0x7f000000)>>24);
			int xsizedraw2 = ((value3 & 0x007f0000)>>16);
			int xx,yy;

			int tilestable[4] = { 8,1,2,4 };
			int ysize2 = ((value3 & 0x0000000c)>>2);
			int xsize2 = ((value3 & 0x00000003)>>0);
			uint32_t xinc,yinc;

			// invalid sprite ysize of 0 tiles
			if (ysize2==0)
				continue;

			// xsize of 0 tiles seems to be a special command to draw tilemaps
			if (xsize2==0)
			{
#ifndef NO_LAYER_ENABLE_TOGGLE
				if (nBurnLayer & 1) // layer disable
				{
#endif
					int tilemapnum = ((value3 & 0x00000030)>>4);
					int startline;// = value2 & 0x3ff;
					int endline;
					int height = (value3 & 0x7f000000)>>24;
					uint32_t * regs;
					regs = RamVReg + 8 + tilemapnum * 4;
					endline = value2;
					startline = endline - height;

					startline &=0x3ff;
					endline &=0x3ff;

					// Urgh, the startline / endline seem to be direct screen co-ordinates regardless of fullscreen zoom
					// which probably means the fullscreen zoom is applied when rendering everything, not aftewards


					if (bg_drawn[tilemapnum]==0)
						for (uint32_t uu=0; uu < 1023; uu++)
							cps3_draw_tilemapsprite_line( uu, regs );
					bg_drawn[tilemapnum] = 1;
#ifndef NO_LAYER_ENABLE_TOGGLE
				}
#endif

			}
			else
			{
#ifndef NO_SPRITE_ENABLE_TOGGLE
				if (~nSpriteEnable & 1)
					continue;
#endif

				ysize2 = tilestable[ysize2];
				xsize2 = tilestable[xsize2];

				xinc = ((xsizedraw2+1)<<16) / ((xsize2<<4));
				yinc = ((ysizedraw2+1)<<16) / ((ysize2<<4));
				xsize2-=1;
				ysize2-=1;

				flipx ^= global_xflip;
				flipy ^= global_yflip;

				if (!flipx)
					xpos2+=((xsizedraw2+1)/2);
				else
					xpos2-=((xsizedraw2+1)/2);

				ypos2+=((ysizedraw2+1)/2);

				if (!flipx)
					xpos2-= (((xsize2+1)*16*xinc)>>16);
				else
					xpos2+= (((xsize2)*16*xinc)>>16);

				if (flipy)
					ypos2-= ((ysize2*16*yinc)>>16);

				count = 0;
				for (xx=0;xx<xsize2+1;xx++)
				{
					int current_xpos;

					if (!flipx)
						current_xpos = (xpos+xpos2+((xx*16*xinc)>>16)  );
					else
						current_xpos = (xpos+xpos2-((xx*16*xinc)>>16));
					//current_xpos +=  rand()&0x3ff;
					current_xpos += gscrollx;
					current_xpos += 1;
					current_xpos &=0x3ff;

					if (current_xpos&0x200)
						current_xpos-=0x400;

					for (yy=0;yy<ysize2+1;yy++)
					{
						int current_ypos;
						int actualpal;
						if (flipy)
							current_ypos = (ypos+ypos2+((yy*16*yinc)>>16));
						else
							current_ypos = (ypos+ypos2-((yy*16*yinc)>>16));
						current_ypos += gscrolly;
						current_ypos = 0x3ff-current_ypos;
						current_ypos -= 17;
						current_ypos &=0x3ff;

						if (current_ypos&0x200)
							current_ypos-=0x400;

						//if ( (whichbpp) && (cpu_getcurrentframe() & 1)) continue;

						/* use the palette value from the main list or the sublists? */
						if (whichpal) actualpal = global_pal;
						else actualpal = pal;

						/* use the bpp value from the main list or the sublists? */
						int color_granularity;
						if (whichbpp)
						{
							if (!global_bpp)
								color_granularity = 8;
							else
								color_granularity = 6;
						}
						else
						{
							if (!bpp)
								color_granularity = 8;
							else
								color_granularity = 6;
						}
						actualpal <<= color_granularity;

						int realtileno = tileno+count;

						if ( realtileno )
						{
							if (global_alpha || alpha)
							{

								// fix jojo's title in it's intro ???
								if ( global_alpha && (global_pal & 0x100))
									actualpal &= 0x0ffff;

								cps3_drawgfxzoom_2(realtileno,actualpal,0^flipx,0^flipy,current_xpos,current_ypos,xinc,yinc, color_granularity);

							}
							else
								cps3_drawgfxzoom_2(realtileno,actualpal,0^flipx,0^flipy,current_xpos,current_ypos,xinc,yinc, 0);
						}
						count++;
					}
				}
			}
		}
	}

	// copy screen from pBurnDraw with zoom
	{
		uint32_t srcx, srcy = 0;
		uint32_t * srcbitmap;
		uint16_t * dstbitmap = (uint16_t * )pBurnDraw;

		for (int rendery=0; rendery<224; rendery++)
		{
			srcbitmap = RamScreen + (srcy >> 16) * 512 * 2;
			srcx=0;
			for (int renderx=0; renderx<cps3_gfx_width; renderx++, dstbitmap ++)
			{
				*dstbitmap = CurPal[ srcbitmap[srcx>>16] ];
				srcx += fsz;
			}
			srcy += fsz;
		}
	}

	// fg layer
#ifndef NO_LAYER_ENABLE_TOGGLE
	if (nBurnLayer & 2) // layer disable
	{
#endif
		// bank select? (sfiii2 intro)
		int count = (ss_bank_base & 0x01000000) ? 0x0000 : 0x0800;
		for (int y=0; y<32; y++)
		{
			for (int x=0; x<64; x++)
			{
				uint32_t data = (RamSS[count]); // +0x800 = 2nd bank, used on sfiii2 intro..
				uint32_t tile = (data >> 16) & 0x1ff;
				int pal = (data & 0x003f) >> 1;
				int flipx = (data & 0x0080) >> 7;
				int flipy = (data & 0x0040) >> 6;
				pal += ss_pal_base << 5;
				tile+=0x200;
				cps3_drawgfxzoom_0(tile,pal,flipx,flipy,x*8,y*8);
				count++;
			}
		}
#ifndef NO_LAYER_ENABLE_TOGGLE
	}
#endif
}

static void cps3SndUpdate()
{
	memset(pBurnSoundOut, 0, nBurnSoundLen << 2);
	signed char * base = (signed char *)chip->rombase;
	cps3_voice *vptr = &chip->voice[0];

	for(int i = 0; i < CPS3_VOICES; i++, vptr++)
	{
		if (chip->key & (1 << i))
		{
			unsigned int start = ((vptr->regs[ 3] << 16) | vptr->regs[ 2]) - 0x400000;
			unsigned int end   = ((vptr->regs[11] << 16) | vptr->regs[10]) - 0x400000;
			unsigned int loop  = ((vptr->regs[ 9] << 16) | vptr->regs[ 7]) - 0x400000;
			unsigned int step  = ( vptr->regs[ 6] * chip->delta ) >> CPS3_SND_LINEAR_SHIFT;

			//int vol_l = ((signed short)vptr->regs[15] * 12) >> 4;
			//int vol_r = ((signed short)vptr->regs[14] * 12) >> 4;
			int vol_l = (signed short)vptr->regs[15];
			int vol_r = (signed short)vptr->regs[14];

			unsigned int pos = vptr->pos;
			unsigned int frac = vptr->frac;

			/* Go through the buffer and add voice contributions */
			signed short * buffer = (signed short *)pBurnSoundOut;

			for (int j=0; j<nBurnSoundLen; j++)
			{
				signed int sample;

				pos += (frac >> 12);
				frac &= 0xfff;

				if (start + pos >= end)
				{
					if (vptr->regs[5])
						pos = loop - start;
					else
					{
						chip->key &= ~(1 << i);
						break;
					}
				}

				// 8bit sample store with 16bit bigend ???
				sample = base[(start + pos) ^ 1];
				frac += step;

#if 1
				int sample_l;

				sample_l = ((sample * vol_r) >> 8) + buffer[0];

				if (sample_l > 32767)
					buffer[0] = 32767;
				else if (sample_l < -32768)
					buffer[0] = -32768;
				else
					buffer[0] = sample_l;

				sample_l = ((sample * vol_l) >> 8) + buffer[1];
				if (sample_l > 32767)
					buffer[1] = 32767;
				else if (sample_l < -32768)
					buffer[1] = -32768;
				else 
					buffer[1] = sample_l;
#else
				buffer[0] += (sample * (vol_l >> 8));
				buffer[1] += (sample * (vol_r >> 8));
#endif

				buffer += 2;
			}

			vptr->pos = pos;
			vptr->frac = frac;
		}
	}
	
}

static int cps3Frame_sfiii2()
{
	if (cps3_reset)
		Cps3Reset();

	if (cps3_palette_change) {
		for(int i=0;i<0x0020000;i++) {
			int data = RamPal[i ];
			int r = (data & 0x001F) << 3;	// Red
			int g = (data & 0x03E0) >> 2;	// Green
			int b = (data & 0x7C00) >> 7;	// Blue
			r |= r >> 5;
			g |= g >> 5;
			b |= b >> 5;
			CurPal[i] = BurnHighCol(r, g, b, 0);
		}
		cps3_palette_change = 0;
	}

	//	EEPROM[0x11] = 0x100 + (EEPROM[0x11] & 0xff);
	//	EEPROM[0x29] = 0x100 + (EEPROM[0x29] & 0xff);

	Cps3Input[0] = 0;
	Cps3Input[1] = 0;
	//Cps3Input[2] = 0;
	Cps3Input[3] = 0;

	for (uint32_t i = 0; i < 16; i++)
	{
		Cps3Input[0] |= (Cps3But1[i] & 1) << i;
		Cps3Input[1] |= (Cps3But2[i] & 1) << i;
		Cps3Input[3] |= (Cps3But3[i] & 1) << i;
	}

	// Clear Opposites
	DrvClearOpposites(&Cps3Input[0]);
	DrvClearOpposites(&Cps3Input[1]);

	//Combine three calls to Sh2Run into one call (3 * 104166 = 312498)
	Sh2Run(312498);

	Sh2SetIRQLine(10, SH2_IRQSTATUS_AUTO);

	Sh2Run(104166);

	Sh2SetIRQLine(12, SH2_IRQSTATUS_AUTO);

	if(pBurnSoundOut)
		cps3SndUpdate();

	if (pBurnDraw)
		DrvDraw_sfiii2();

	return 0;
}

static int cps3Frame()
{
	if (cps3_reset)
		Cps3Reset();

	if (cps3_palette_change) {
		for(int i=0;i<0x0020000;i++) {
			int data = RamPal[i ];
			int r = (data & 0x001F) << 3;	// Red
			int g = (data & 0x03E0) >> 2;	// Green
			int b = (data & 0x7C00) >> 7;	// Blue
			r |= r >> 5;
			g |= g >> 5;
			b |= b >> 5;
			CurPal[i] = BurnHighCol(r, g, b, 0);
		}
		cps3_palette_change = 0;
	}

//	EEPROM[0x11] = 0x100 + (EEPROM[0x11] & 0xff);
//	EEPROM[0x29] = 0x100 + (EEPROM[0x29] & 0xff);

	Cps3Input[0] = 0;
	Cps3Input[1] = 0;
	//Cps3Input[2] = 0;
	Cps3Input[3] = 0;
	for (int i= 0; i < 16; i++)
	{
		Cps3Input[0] |= (Cps3But1[i] & 1) << i;
		Cps3Input[1] |= (Cps3But2[i] & 1) << i;
		Cps3Input[3] |= (Cps3But3[i] & 1) << i;
	}

	// Clear Opposites
	DrvClearOpposites(&Cps3Input[0]);
	DrvClearOpposites(&Cps3Input[1]);

	//Combine three calls to Sh2Run into one call (3 * 104166 = 312498)
	Sh2Run(312498);

	Sh2SetIRQLine(10, SH2_IRQSTATUS_AUTO);

	Sh2Run(104166);

	Sh2SetIRQLine(12, SH2_IRQSTATUS_AUTO);

	if(pBurnSoundOut)
		cps3SndUpdate();

	if (pBurnDraw)
		DrvDraw();

	return 0;
}

static int cps3SndScan(int nAction)
{
	if (nAction & ACB_DRIVER_DATA)
	{
		SCAN_VAR( chip->voice );
		SCAN_VAR( chip->key );
		
	}
	return 0;
}

static int cps3Scan(int nAction, int *pnMin)
{
	if (pnMin) *pnMin =  0x029672;

	struct BurnArea ba;

	if (nAction & ACB_NVRAM) {				// Save EEPROM configuration
		// Scan nvram, backup this can save inner config by F2 ???
		ba.Data		= EEPROM;
		ba.nLen		= 0x0000400;
		ba.nAddress = 0;
		ba.szName	= "EEPROM RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM) {

		ba.Data		= RamMain;
		ba.nLen		= 0x0080000;
		ba.nAddress = 0;
		ba.szName	= "Main RAM";
		BurnAcb(&ba);

		ba.Data		= RamSpr;
		ba.nLen		= 0x0080000;
		ba.nAddress = 0;
		ba.szName	= "Sprite RAM";
		BurnAcb(&ba);

		ba.Data		= RamSS;
		ba.nLen		= 0x0010000;
		ba.nAddress = 0;
		ba.szName	= "Char ROM";
		BurnAcb(&ba);

		ba.Data		= RamVReg;
		ba.nLen		= 0x0000100;
		ba.nAddress = 0;
		ba.szName	= "Video REG";
		BurnAcb(&ba);

		ba.Data		= RamC000;
		ba.nLen		= 0x0000400 * 2;
		ba.nAddress = 0;
		ba.szName	= "RAM C000";
		BurnAcb(&ba);

		ba.Data		= RamPal;
		ba.nLen		= 0x0040000;
		ba.nAddress = 0;
		ba.szName	= "Palette";
		BurnAcb(&ba);

		ba.Data		= RamCRam;
		ba.nLen		= 0x0800000;
		ba.nAddress = 0;
		ba.szName	= "Sprite ROM";
		BurnAcb(&ba);

/*		// so huge. need not backup it while NOCD
		// otherwise, need backup gfx also
		ba.Data		= RomGame;
		ba.nLen		= 0x1000000;
		ba.nAddress = 0;
		ba.szName	= "Game ROM";
		BurnAcb(&ba);
*/
	}

	if (nAction & ACB_DRIVER_DATA) {

		Sh2Scan(nAction);
		cps3SndScan(nAction);

		SCAN_VAR(Cps3Input);

		SCAN_VAR(ss_bank_base);
		SCAN_VAR(ss_pal_base);
		SCAN_VAR(cram_bank);
		SCAN_VAR(cps3_current_eeprom_read);
		SCAN_VAR(gfxflash_bank);

		SCAN_VAR(paldma_source);
		SCAN_VAR(paldma_dest);
		SCAN_VAR(paldma_fade);
		SCAN_VAR(paldma_length);

		SCAN_VAR(chardma_source);
		SCAN_VAR(chardma_table_address);

		//SCAN_VAR(main_flash);

		//SCAN_VAR(last_normal_byte);
		//SCAN_VAR(lastb);
		//SCAN_VAR(lastb2);

		if (nAction & ACB_WRITE)
		{
			// rebuild current palette
			cps3_palette_change = 1;

			// remap RamCRam
			Sh2MapMemory(((uint8_t *)RamCRam) + (cram_bank << 20), 0x04100000, 0x041fffff, SM_RAM);

		}
	}

	return 0;
}

#include "d_cps3_.h"
