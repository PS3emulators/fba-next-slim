// FB Alpha - Emulator for MC68000/Z80 based arcade games
//            Refer to the "license.txt" file for more info

#ifndef BURNER_H
#define BURNER_H

#define MAX_PATH 260

#if defined (_XBOX)
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#endif

#include <limits.h>
#include <stdarg.h>
#ifndef SN_TARGET_PS3
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>

#include "title.h"
#ifdef __LIBSNES__
#include "../burn/burn.h"
#else
#include "burn.h"
#endif

#include "utility.h"

// Macro to make quoted strings
#define MAKE_STRING_2(s) #s
#define MAKE_STRING(s) MAKE_STRING_2(s)

#define BZIP_MAX (8)								// Maximum zip files to search through
#define DIRS_MAX (14)								// Maximum number of directories to search

#define DEFAULT_IMAGE_WIDTH		(304)
#define DEFAULT_IMAGE_HEIGHT	(224)

// ---------------------------------------------------------------------------
// OS dependent functionality

#if defined (_XBOX)
#include "burner_Xbox360.h"
#include "localise.h"
#elif defined (SN_TARGET_PS3)
#include "burner_ps3.h"
#include "localise.h"
#elif defined (_WIN32) && !defined(__LIBSNES__)
#include "burner_win32.h"
#elif defined (BUILD_SDL)
#include "burner_sdl.h"
#endif

// ---------------------------------------------------------------------------
// OS independent functionality

#if defined(SN_TARGET_PS3)
#include "interface-ps3.h"
#elif defined (__LIBSNES__)
#else
#include "interface.h"
#endif

// gami.cpp
extern struct GameInp* GameInp;
extern unsigned int nGameInpCount;
extern unsigned int nMacroCount;
extern unsigned int nMaxMacro;

extern int nAnalogSpeed;
extern int nFireButtons;

extern bool bStreetFighterLayout;
extern bool bLeftAltkeyMapped;

int GameInpInit();
int GameInpExit();
char * InputCodeDesc(int c);
const char * InpToDesc(struct GameInp* pgi);
char * InpMacroToDesc(struct GameInp* pgi);
void GameInpCheckLeftAlt();
void GameInpCheckMouse();
int GameInpBlank(int bDipSwitch);
int GameInputAutoIni(int nPlayer, const char * lpszFile, bool bOverWrite);
int GameInpDefault();
int GameInpWrite(FILE* h, bool bWriteConst = true);
int GameInpRead(char * szVal, bool bOverWrite);
int GameInpMacroRead(char * szVal, bool bOverWrite);
int GameInpCustomRead(char * szVal, bool bOverWrite);

// Player Default Controls
extern int nPlayerDefaultControls[4];
extern char szPlayerDefaultIni[4][MAX_PATH];

// cong.cpp
extern const int nConfigMinVersion;					// Minimum version of application for which input files are valid
int ConfigGameLoad(bool bOverWrite);				// char* lpszName = NULL
int ConfigGameSave(bool bSave);

// gamc.cpp
int GamcMisc(struct GameInp* pgi, char* szi, int nPlayer);
int GamcAnalogKey(struct GameInp* pgi, char* szi, int nPlayer, int nSlide);
int GamcAnalogJoy(struct GameInp* pgi, char* szi, int nPlayer, int nJoy, int nSlide);
int GamcXboxPlayer(struct GameInp* pgi, char* szi, int nPlayer, int nDevice);
int GamcPlayer(struct GameInp* pgi, char* szi, int nPlayer, int nDevice);
int GamcPlayerHotRod(struct GameInp* pgi, char* szi, int nPlayer, int nFlags, int nSlide);
int GamcPlayerHori(struct GameInp* pgi, char* szi, int nPlayer, int nSlide);

// misc.cpp
extern int bcolorAdjust;
extern int color_gamma;
extern int color_brightness;
extern int color_contrast;
extern int color_grayscale;
extern int color_invert;

void colorAdjust(int& r, int& g, int& b);
int SetBurnHighCol(int nDepth);
char* decorateGameName(unsigned int nBurnDrv);

// autofire.cpp
extern int nAutofireEnabled;
extern unsigned int autofireDelay;
extern unsigned int autofireDefaultDelay;
extern void UpdateConsole(const char *text);

// dat.cpp
int write_xmlfile(const char * szFilename, FILE* file);
int write_datfile(FILE* file);
int create_datfile(char* szFilename, int type);

// sshot.cpp
unsigned char* ConvertVidImage(unsigned char* src, int bFlipVertical = 0);
int MakeScreenShot(bool bScrShot, int Type);

// state.cpp
int BurnStateLoadEmbed(FILE* fp, int nOffset, int bAll, int (*pLoadGame)());
int BurnStateLoad(const char * szName, int bAll, int (*pLoadGame)());
int BurnStateSaveEmbed(FILE* fp, int nOffset, int bAll);
int BurnStateSave(char* szName, int bAll);

// statec.cpp
int BurnStateCompress(unsigned char** pDef, int* pnDefLen, int bAll);
int BurnStateDecompress(unsigned char* Def, int nDefLen, int bAll);

// archive.cpp
struct ArcEntry { char* szName; unsigned int nLen; unsigned int nCrc; };

enum ARCTYPE { ARC_NONE = -1, ARC_ZIP = 0, ARC_7Z, ARC_NUM };

int archiveCheck(char* name, int zipOnly = 0);
int archiveOpen(const char* archive);
int archiveOpenA(const char* archive);
int archiveClose();
int archiveGetList(ArcEntry** list, int* count = NULL);
int archiveLoadFile(unsigned char* dest, int len, int entry, int* wrote = NULL);
int archiveLoadOneFile(const char* arc, const char* file, void** dest, int* wrote = NULL);

// barchive.cpp
enum BARC_STATUS { BARC_STATUS_OK = 0, BARC_STATUS_BADDATA, BARC_STATUS_ERROR };

int BArchiveOpen(bool);
int BArchiveClose();
int BArchiveStatus();

// for changing sound track
void NeoZ80Cmd(uint16_t sound_code);
void CpsSoundCmd(uint16_t sound_code);
void QSoundCMD(uint16_t sound_code);

int AudWriteSilence(int);

#endif
