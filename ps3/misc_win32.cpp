// misc win32 functions
#include "burner.h"
#include "vid_psgl.h"
#include "audio_driver.h"

void setWindowAspect(bool first_boot)
{
	custom_aspect_ratio_mode = 0;		//assume this is 0

	switch(nVidScrnAspectMode)
	{
		case ASPECT_RATIO_4_3:
			nVidScrnAspectX = 4;
			nVidScrnAspectY = 3;
			break;
		case ASPECT_RATIO_5_4:
			nVidScrnAspectX = 5;
			nVidScrnAspectY = 4;
			break;
		case ASPECT_RATIO_7_5:
			nVidScrnAspectX = 7;
			nVidScrnAspectY = 5;
			break;
		case ASPECT_RATIO_8_7:
			nVidScrnAspectX = 8;
			nVidScrnAspectY = 7;
			break;
		case ASPECT_RATIO_10_7:
			nVidScrnAspectX = 10;
			nVidScrnAspectY = 7;
			break;
		case ASPECT_RATIO_11_8:
			nVidScrnAspectX = 11;
			nVidScrnAspectY = 8;
			break;
		case ASPECT_RATIO_12_7:
			nVidScrnAspectX = 12;
			nVidScrnAspectY = 7;
			break;
		case ASPECT_RATIO_16_9:
			nVidScrnAspectX = 16;
			nVidScrnAspectY = 9;
			break;
		case ASPECT_RATIO_16_10:
			nVidScrnAspectX = 16;
			nVidScrnAspectY = 10;
			break;
		case ASPECT_RATIO_16_15:
			nVidScrnAspectX = 16;
			nVidScrnAspectY = 15;
			break;
		case ASPECT_RATIO_19_14:
			nVidScrnAspectX = 19;
			nVidScrnAspectY = 14;
			break;
		case ASPECT_RATIO_1_1:
			nVidScrnAspectX = 1;
			nVidScrnAspectY = 1;
			break;
		case ASPECT_RATIO_2_1:
			nVidScrnAspectX = 2;
			nVidScrnAspectY = 1;
			break;
		case ASPECT_RATIO_3_2:
			nVidScrnAspectX = 3;
			nVidScrnAspectY = 2;
			break;
		case ASPECT_RATIO_3_4:
			nVidScrnAspectX = 3;
			nVidScrnAspectY = 4;
			break;
		case ASPECT_RATIO_CUSTOM:
			custom_aspect_ratio_mode = 1;
			break;
		case ASPECT_RATIO_AUTO:
			if(bDrvOkay)
			{
				int width, height;
				BurnDrvGetFullSize(&width, &height);
				unsigned len = width < height ? width : height;
				unsigned highest = 1;
				for (unsigned i = 1; i < len; i++)
				{
					if ((width % i) == 0 && (height % i) == 0)
						highest = i;
				}

				nVidScrnAspectX = width / highest;
				nVidScrnAspectY = height / highest;
			}
			break;
		case ASPECT_RATIO_AUTO_FBA:
			break; 
	}

	m_ratio = ((float)nVidScrnAspectX/nVidScrnAspectY);
}

// ---------------------------------------------------------------------------
// For DAT files printing and Kaillera windows

char * decorateGameName(unsigned int drv)
{
	if (drv >= nBurnDrvCount)
		return "";

	unsigned int nOldBurnDrv = nBurnDrvSelect;
	nBurnDrvSelect = drv;

	// get game full name
	static char szDecoratedName[1024] = "";
	strcpy(szDecoratedName, BurnDrvGetTextA(DRV_FULLNAME));

	// get game extra info
	char szGameInfo[256] = " [";
	bool hasInfo = false;

	if (BurnDrvGetFlags() & BDF_PROTOTYPE)
	{
		strcat(szGameInfo, "prototype");
		hasInfo = true;
	}
	if (BurnDrvGetFlags() & BDF_BOOTLEG)
	{
		strcat(szGameInfo, "bootleg");
		hasInfo = true;
	}
	if (BurnDrvGetTextA(DRV_COMMENT))
	{
		strcat(szGameInfo, BurnDrvGetTextA(DRV_COMMENT));
		hasInfo = true;
	}

	if (hasInfo)
	{
		strcat(szGameInfo, "]");
		strcat(szDecoratedName, szGameInfo);
	}

	nBurnDrvSelect = nOldBurnDrv;

	return szDecoratedName;
}

// rom util
static inline int findRomByName(const char* name, ArcEntry* list, int count)
{
	if (!name || !list)
		return -1;

	// Find the rom named name in the List
	int i = 0;
	do
	{
		if (list->szName && !strcasecmp(name, getFilenameA(list->szName)))
			return i;
		i++;
		list++;
	}while(i < count);
	return -1; // couldn't find the rom
}

static inline int findRomByCrc(unsigned int crc, ArcEntry* list, int count)
{
	if (!list)
		return -1;

	// Find the rom named name in the List
	int i = 0;
	do
	{
		if (crc == list->nCrc)
			return i;
		i++;
		list++;
	}while(i < count);

	return -1; // couldn't find the rom
}

// Find rom number i from the pBzipDriver game
int findRom(int i, ArcEntry* list, int count)
{
	BurnRomInfo ri;
	memset(&ri, 0, sizeof(ri));

	int nRet = BurnDrvGetRomInfo(&ri, i);
	if (nRet != 0) // Failure: no such rom
		return -2;

	if (ri.nCrc)   // Search by crc first
	{
		nRet = findRomByCrc(ri.nCrc, list, count);
		if (nRet >= 0)
			return nRet;
	}

	int nAka = 0;
	do
	{	// Failing that, search for possible names
		char* szPossibleName = NULL;
		nRet = BurnDrvGetRomName(&szPossibleName, i, nAka);

		if (nRet) // No more rom names
			break;

		nRet = findRomByName(szPossibleName, list, count);

		if (nRet >= 0)
			return nRet;

		nAka++;
	}while(nAka < 0x10000);

	return -1; // Couldn't find the rom
}
