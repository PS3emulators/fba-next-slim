#include <cell/audio.h>
#include <cell/sysmodule.h>
#include "cellframework2/audio/stream.h"
#include "audio_driver.h"
#include "burner.h"

extern cell_audio_handle_t audio_handle;
extern const struct cell_audio_driver *	driver;

int		bAudSetSampleRate = 48010;
int		nAudSampleRate = 48010;	// Sample rate
int		nAudSegCount = 6;	// Segments in the pdsbLoop buffer
int		nAudSegLen = 0;		// Segment length in samples (calculated from Rate/Fps)
int		nAudAllocSegLen = 0;	// Allocated segment length in samples
bool		bAudOkay = false;	// True if sound was inited okay
bool		bAudPlaying = false;
bool		bAudReinit = false;
int16_t *	pAudNextSound = NULL;	// The next sound seg we will add to the sample loop

int audio_new(void)
{
	driver = &cell_audio_audioport;
	audio_handle = NULL;

	return 0;
}

int audio_exit()
{ 
	bAudOkay = bAudPlaying = false;

	if (audio_handle)
	{
		driver->free(audio_handle);
		audio_handle = NULL;
	}

	if (pAudNextSound)
	{
		free(pAudNextSound);
		pAudNextSound = NULL;
	}

	return 0;
}

int audio_init(int samplerate)
{
	switch(samplerate)
	{
		case SAMPLERATE_11KHZ:
			nAudSegLen = 184;
			nAudAllocSegLen = 736;
			nAudSampleRate = SAMPLERATE_11KHZ;
			break;
		case SAMPLERATE_22KHZ:
			nAudSegLen = 368;
			nAudAllocSegLen = 1472;
			nAudSampleRate = SAMPLERATE_22KHZ;
			break;
		case SAMPLERATE_44KHZ:
			nAudSegLen = 735;
			nAudAllocSegLen = 2940;
			nAudSampleRate = SAMPLERATE_44KHZ;
			break;
		case 48000:
		case SAMPLERATE_48KHZ: 
			nAudSegLen = 801;
			nAudAllocSegLen = 12800;
			samplerate = 48000;
			nAudSampleRate = SAMPLERATE_48KHZ;
			break;

	}

	cell_audio_params params;
	memset(&params, 0, sizeof(params));
	params.channels = 2;
	params.samplerate = samplerate;
	params.buffer_size = 8192;
	params.userdata = NULL;
	params.device = NULL;
	audio_handle = driver->init(&params);

	// The next sound block to put in the stream
	pAudNextSound = (int16_t*)realloc(pAudNextSound, nAudAllocSegLen);

	if (pAudNextSound == NULL)
	{
		audio_exit();
		return 1;
	}

	audio_blank();
	bAudOkay = true;

	return 0;
}
