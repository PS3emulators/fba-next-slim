#include "burnint.h"
#include "burn_sound.h"
#include "namco_snd.h"

#define MAX_VOICES 8
#define MAX_VOLUME 16
#define INTERNAL_RATE	192000
#define MIXLEVEL	(1 << (16 - 4 - 4))
#define OUTPUT_LEVEL(n)		((n) * MIXLEVEL / chip->num_voices)
#define WAVEFORM_POSITION(n)	(((n) >> chip->f_fracbits) & 0x1f)

unsigned char* NamcoSoundProm = NULL;

typedef struct
{
	UINT32 frequency;
	UINT32 counter;
	INT32 volume[2];
	INT32 noise_sw;
	INT32 noise_state;
	INT32 noise_seed;
	UINT32 noise_counter;
	INT32 noise_hold;
	INT32 waveform_select;
} sound_channel;

static UINT8 *namco_soundregs;
static UINT8 *namco_wavedata;

struct namco_sound
{
	sound_channel channel_list[MAX_VOICES];
	sound_channel *last_channel;

	int wave_size;
	INT32 num_voices;
	INT32 sound_enable;
	int namco_clock;
	int sample_rate;
	int f_fracbits;
	int stereo;

	INT16 *waveform[MAX_VOLUME];
	
	int update_step;
};

static struct namco_sound *chip = NULL;

static void update_namco_waveform(int offset, UINT8 data)
{
	if (chip->wave_size == 1)
	{
		INT16 wdata;
		int v;

		/* use full byte, first 4 high bits, then low 4 bits */
		for (v = 0; v < MAX_VOLUME; v++)
		{
			wdata = ((data >> 4) & 0x0f) - 8;
			chip->waveform[v][offset * 2] = OUTPUT_LEVEL(wdata * v);
			wdata = (data & 0x0f) - 8;
			chip->waveform[v][offset * 2 + 1] = OUTPUT_LEVEL(wdata * v);
		}
	}
	else
	{
		int v;

		/* use only low 4 bits */
		for (v = 0; v < MAX_VOLUME; v++)
			chip->waveform[v][offset] = OUTPUT_LEVEL(((data & 0x0f) - 8) * v);
	}
}

static inline UINT32 namco_update_one(short *buffer, int length, const INT16 *wave, UINT32 counter, UINT32 freq)
{
	while (length-- > 0)
	{
		*buffer++ += wave[WAVEFORM_POSITION(counter)];
		*buffer++ += wave[WAVEFORM_POSITION(counter)];
		counter += freq * chip->update_step;
	}

	return counter;
}

static inline UINT32 namco_stereo_update_one(short *buffer, int length, const INT16 *wave, UINT32 counter, UINT32 freq)
{
	while (length-- > 0)
	{
		*buffer += wave[WAVEFORM_POSITION(counter)];
		counter += freq * chip->update_step;
		buffer +=2;
	}

	return counter;
}

void NamcoSoundUpdate(short* buffer, int length)
{
	sound_channel *voice;

	/* zap the contents of the buffer */
	memset(buffer, 0, length * sizeof(*buffer) * 2);

	/* if no sound, we're done */
	if (chip->sound_enable == 0)
		return;

	/* loop over each voice and add its contribution */
	for (voice = chip->channel_list; voice < chip->last_channel; voice++)
	{
		short *mix = buffer;
		int v = voice->volume[0];

		if (voice->noise_sw)
		{
			int f = voice->frequency & 0xff;

			/* only update if we have non-zero volume and frequency */
			if (v && f)
			{
				int hold_time = 1 << (chip->f_fracbits - 16);
				int hold = voice->noise_hold;
				UINT32 delta = f << 4;
				UINT32 c = voice->noise_counter;
				INT16 noise_data = OUTPUT_LEVEL(0x07 * (v >> 1));
				int i;

				/* add our contribution */
				for (i = 0; i < length; i++)
				{
					int cnt;

					if (voice->noise_state)
						*mix++ += noise_data;
					else
						*mix++ -= noise_data;

					if (hold)
					{
						hold--;
						continue;
					}

					hold = 	hold_time;

					c += delta;
					cnt = (c >> 12);
					c &= (1 << 12) - 1;
					for( ;cnt > 0; cnt--)
					{
						if ((voice->noise_seed + 1) & 2) voice->noise_state ^= 1;
						if (voice->noise_seed & 1) voice->noise_seed ^= 0x28000;
						voice->noise_seed >>= 1;
					}
				}

				/* update the counter and hold time for this voice */
				voice->noise_counter = c;
				voice->noise_hold = hold;
			}
		}
		else
		{
			/* only update if we have non-zero volume and frequency */
			if (v && voice->frequency)
			{
				const INT16 *w = &chip->waveform[v][voice->waveform_select * 32];

				/* generate sound into buffer and update the counter for this voice */
				voice->counter = namco_update_one(mix, length, w, voice->counter, voice->frequency);
			}
		}
	}
}

void NamcoSoundUpdateStereo(short* buffer, int length)
{
	sound_channel *voice;

	/* zap the contents of the buffers */
	memset(buffer, 0, length * 2 * sizeof(*buffer));

	/* if no sound, we're done */
	if (chip->sound_enable == 0)
		return;

	/* loop over each voice and add its contribution */
	for (voice = chip->channel_list; voice < chip->last_channel; voice++)
	{
		short *lrmix = buffer;
		int lv = voice->volume[0];
		int rv = voice->volume[1];

		if (voice->noise_sw)
		{
			int f = voice->frequency & 0xff;

			/* only update if we have non-zero volume and frequency */
			if ((lv || rv) && f)
			{
				int hold_time = 1 << (chip->f_fracbits - 16);
				int hold = voice->noise_hold;
				UINT32 delta = f << 4;
				UINT32 c = voice->noise_counter;
				INT16 l_noise_data = OUTPUT_LEVEL(0x07 * (lv >> 1));
				INT16 r_noise_data = OUTPUT_LEVEL(0x07 * (rv >> 1));
				int i;

				/* add our contribution */
				for (i = 0; i < length; i++)
				{
					int cnt;

					if (voice->noise_state)
					{
						*lrmix++ += l_noise_data;
						*lrmix++ += r_noise_data;
					}
					else
					{
						*lrmix++ -= l_noise_data;
						*lrmix++ -= r_noise_data;
					}

					if (hold)
					{
						hold--;
						continue;
					}

					hold =	hold_time;

					c += delta;
					cnt = (c >> 12);
					c &= (1 << 12) - 1;
					for( ;cnt > 0; cnt--)
					{
						if ((voice->noise_seed + 1) & 2) voice->noise_state ^= 1;
						if (voice->noise_seed & 1) voice->noise_seed ^= 0x28000;
						voice->noise_seed >>= 1;
					}
				}

				/* update the counter and hold time for this voice */
				voice->noise_counter = c;
				voice->noise_hold = hold;
			}
		}
		else
		{
			/* only update if we have non-zero frequency */
			if (voice->frequency)
			{
				/* save the counter for this voice */
				UINT32 c = voice->counter;

				/* only update if we have non-zero left volume */
				if (lv)
				{
					const INT16 *lw = &chip->waveform[lv][voice->waveform_select * 32];

					/* generate sound into the buffer */
					c = namco_stereo_update_one(lrmix + 0, length, lw, voice->counter, voice->frequency);
				}

				/* only update if we have non-zero right volume */
				if (rv)
				{
					const INT16 *rw = &chip->waveform[rv][voice->waveform_select * 32];

					/* generate sound into the buffer */
					c = namco_stereo_update_one(lrmix + 1, length, rw, voice->counter, voice->frequency);
				}

				/* update the counter for this voice */
				voice->counter = c;
			}
		}
	}
}

void NamcoSoundWrite(unsigned int offset, unsigned char data)
{
	sound_channel *voice;
	int ch;

	data &= 0x0f;
	if (namco_soundregs[offset] == data)
		return;

	/* set the register */
	namco_soundregs[offset] = data;

	if (offset < 0x10)
		ch = (offset - 5) / 5;
	else if (offset == 0x10)
		ch = 0;
	else
		ch = (offset - 0x11) / 5;

	if (ch >= chip->num_voices)
		return;

	/* recompute the voice parameters */
	voice = chip->channel_list + ch;
	switch (offset - ch * 5)
	{
	case 0x05:
		voice->waveform_select = data & 7;
		break;

	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
		/* the frequency has 20 bits */
		/* the first voice has extra frequency bits */
		voice->frequency = (ch == 0) ? namco_soundregs[0x10] : 0;
		voice->frequency += (namco_soundregs[ch * 5 + 0x11] << 4);
		voice->frequency += (namco_soundregs[ch * 5 + 0x12] << 8);
		voice->frequency += (namco_soundregs[ch * 5 + 0x13] << 12);
		voice->frequency += (namco_soundregs[ch * 5 + 0x14] << 16);	/* always 0 */
		break;

	case 0x15:
		voice->volume[0] = data;
		break;
	}
}

static void namcos1_sound_write(int offset, int data)
{
	/* verify the offset */
	if (offset > 63)
	{
	//	logerror("NAMCOS1 sound: Attempting to write past the 64 registers segment\n");
		return;
	}

	if (namco_soundregs[offset] == data)
		return;

	/* set the register */
	namco_soundregs[offset] = data;

	int ch = offset / 8;
	if (ch >= chip->num_voices)
		return;

	/* recompute the voice parameters */
	sound_channel *voice = chip->channel_list + ch;

	switch (offset - ch * 8)
	{
		case 0x00:
			voice->volume[0] = data & 0x0f;
			break;

		case 0x01:
			voice->waveform_select = (data >> 4) & 15;
		case 0x02:
		case 0x03:
			/* the frequency has 20 bits */
			voice->frequency = (namco_soundregs[ch * 8 + 0x01] & 15) << 16;	/* high bits are from here */
			voice->frequency += namco_soundregs[ch * 8 + 0x02] << 8;
			voice->frequency += namco_soundregs[ch * 8 + 0x03];
			break;

		case 0x04:
			voice->volume[1] = data & 0x0f;
	
			int nssw = ((data & 0x80) >> 7);
			if (++voice == chip->last_channel)
				voice = chip->channel_list;
			voice->noise_sw = nssw;
			break;
	}
}

void namcos1_custom30_write(int offset, int data)
{
	if (offset < 0x100)
	{
		if (namco_wavedata[offset] != data)
		{
			namco_wavedata[offset] = data;

			/* update the decoded waveform table */
			update_namco_waveform(offset, data);
		}
	}
	else if (offset < 0x140) {
		namco_wavedata[offset] = data;
		namcos1_sound_write(offset - 0x100, data);
	} else
		namco_wavedata[offset] = data;
}

unsigned char namcos1_custom30_read(int offset)
{
	return namco_wavedata[offset];
}

static int build_decoded_waveform()
{
	INT16 *p;
	int size;
	int offset;
	int v;

	if (NamcoSoundProm != NULL)
		namco_wavedata = NamcoSoundProm;

	/* 20pacgal has waves in RAM but old sound system */
	if (NamcoSoundProm == NULL && chip->num_voices != 3)
	{
		chip->wave_size = 1;
		size = 32 * 16;		/* 32 samples, 16 waveforms */
	}
	else
	{
		chip->wave_size = 0;
		size = 32 * 8;		/* 32 samples, 8 waveforms */
	}

	p = (INT16*)malloc(size * MAX_VOLUME * sizeof (INT16));

	for (v = 0; v < MAX_VOLUME; v++)
	{
		chip->waveform[v] = p;
		p += size;
	}

	/* We need waveform data. It fails if region is not specified. */
	if (namco_wavedata)
	{
		for (offset = 0; offset < 256; offset++)
			update_namco_waveform(offset, namco_wavedata[offset]);
	}

	return 0;
}

void NamcoSoundInit(int clock)
{
	int clock_multiple;
	sound_channel *voice;
	
	chip = (struct namco_sound*)malloc(sizeof(*chip));
	memset(chip, 0, sizeof(*chip));
	
	namco_soundregs = (UINT8*)malloc(0x40);
	memset(namco_soundregs, 0, 0x40);

	chip->num_voices = 3;
	chip->last_channel = chip->channel_list + chip->num_voices;
	chip->stereo = 0;

	/* adjust internal clock */
	chip->namco_clock = clock;
	for (clock_multiple = 0; chip->namco_clock < INTERNAL_RATE; clock_multiple++)
		chip->namco_clock *= 2;

	chip->f_fracbits = clock_multiple + 15;

	/* adjust output clock */
	chip->sample_rate = chip->namco_clock;

	/* build the waveform table */
	if (build_decoded_waveform()) return;
	
	/* start with sound enabled, many games don't have a sound enable register */
	chip->sound_enable = 1;

	/* reset all the voices */
	for (voice = chip->channel_list; voice < chip->last_channel; voice++)
	{
		voice->frequency = 0;
		voice->volume[0] = voice->volume[1] = 0;
		voice->waveform_select = 0;
		voice->counter = 0;
		voice->noise_sw = 0;
		voice->noise_state = 0;
		voice->noise_seed = 1;
		voice->noise_counter = 0;
		voice->noise_hold = 0;
	}
	
	chip->update_step = INTERNAL_RATE / nBurnSoundRate;
}

void NamcoSoundExit()
{
	if (chip) {
		free(chip);
		chip = NULL;
	}
	
	if (namco_soundregs) {
		free(namco_soundregs);
		namco_soundregs = NULL;
	}
}

void NamcoSoundScan(int nAction,int *pnMin)
{
	struct BurnArea ba;
	char szName[16];
	
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}
	
	if (pnMin != NULL) {
		*pnMin = 0x029707;
	}
	
/*	sprintf(szName, "NamcoSound");
	ba.Data		= &chip;
	ba.nLen		= sizeof(chip);
	ba.nAddress = 0;
	ba.szName	= szName;
	BurnAcb(&ba);*/
	
	sprintf(szName, "NamcoSoundRegs");
	ba.Data		= namco_soundregs;
	ba.nLen		= 0x40;
	ba.nAddress = 0;
	ba.szName	= szName;
	BurnAcb(&ba);
}

