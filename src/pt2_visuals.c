// for finding memory leaks in debug mode with Visual Studio 
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h> // modf()
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <SDL2/SDL_syswm.h>
#endif
#include <stdint.h>
#include <stdbool.h>
#ifndef _WIN32
#include <unistd.h> // usleep()
#endif
#include <ctype.h> // tolower()
#include "pt2_header.h"
#include "pt2_keyboard.h"
#include "pt2_mouse.h"
#include "pt2_audio.h"
#include "pt2_helpers.h"
#include "pt2_textout.h"
#include "pt2_tables.h"
#include "pt2_module_loader.h"
#include "pt2_module_saver.h"
#include "pt2_sample_loader.h"
#include "pt2_sample_saver.h"
#include "pt2_pattern_viewer.h"
#include "pt2_sampler.h"
#include "pt2_diskop.h"
#include "pt2_visuals.h"
#include "pt2_scopes.h"
#include "pt2_edit.h"
#include "pt2_pat2smp.h"
#include "pt2_mod2wav.h"
#include "pt2_config.h"
#include "pt2_bmp.h"
#include "pt2_sampling.h"
#include "pt2_chordmaker.h"
#include "pt2_hpc.h"

typedef struct sprite_t
{
	bool visible;
	int8_t pixelType;
	int16_t newX, newY, x, y, w, h;
	uint32_t colorKey, *refreshBuffer;
	const void *data;
} sprite_t;

static uint32_t vuMetersBg[4 * (10 * 48)];

sprite_t sprites[SPRITE_NUM]; // globalized

static const uint16_t cursorPosTable[24] =
{
	 30,  54,  62,  70,  78,  86,
	102, 126, 134, 142, 150, 158,
	174, 198, 206, 214, 222, 230,
	246, 270, 278, 286, 294, 302
};

void updateSongInfo1(void);
void updateSongInfo2(void);
void updateSampler(void);
void updatePatternData(void);
void updateMOD2WAVDialog(void);

void blit32(int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t *src)
{
	const uint32_t *srcPtr = src;
	uint32_t *dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];

	for (int32_t yy = 0; yy < h; yy++)
	{
		memcpy(dstPtr, srcPtr, w * sizeof (int32_t));

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void putPixel(int32_t x, int32_t y, const uint32_t pixelColor)
{
	video.frameBuffer[(y * SCREEN_W) + x] = pixelColor;
}

void hLine(int32_t x, int32_t y, int32_t w, const uint32_t pixelColor)
{
	uint32_t *dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];
	for (int32_t xx = 0; xx < w; xx++)
		dstPtr[xx] = pixelColor;
}

void vLine(int32_t x, int32_t y, int32_t h, const uint32_t pixelColor)
{
	uint32_t *dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];
	for (int32_t yy = 0; yy < h; yy++)
	{
		*dstPtr = pixelColor;
		dstPtr += SCREEN_W;
	}
}

void drawFramework1(int32_t x, int32_t y, int32_t w, int32_t h)
{
	hLine(x, y, w-1, video.palette[PAL_BORDER]);
	vLine(x, y+1, h-2, video.palette[PAL_BORDER]);
	hLine(x+1, y+h-1, w-1, video.palette[PAL_GENBKG2]);
	vLine(x+w-1, y+1, h-2, video.palette[PAL_GENBKG2]);

	putPixel(x, y+h-1, video.palette[PAL_GENBKG]);
	putPixel(x+w-1, y, video.palette[PAL_GENBKG]);

	fillRect(x+1, y+1, w-2, h-2, video.palette[PAL_GENBKG]);
}

void drawFramework2(int32_t x, int32_t y, int32_t w, int32_t h)
{
	hLine(x, y, w-1, video.palette[PAL_GENBKG2]);
	vLine(x, y+1, h-2, video.palette[PAL_GENBKG2]);
	hLine(x+1, y+h-1, w-1, video.palette[PAL_BORDER]);
	vLine(x+w-1, y+1, h-2, video.palette[PAL_BORDER]);

	putPixel(x, y+h-1, video.palette[PAL_GENBKG]);
	putPixel(x+w-1, y, video.palette[PAL_GENBKG]);

	fillRect(x+1, y+1, w-2, h-2, video.palette[PAL_BACKGRD]);
}

void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t pixelColor)
{
	uint32_t *dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];

	for (int32_t yy = 0; yy < h; yy++)
	{
		for (int32_t xx = 0; xx < w; xx++)
			dstPtr[xx] = pixelColor;

		dstPtr += SCREEN_W;
	}
}

void drawButton(int32_t x, int32_t y, int32_t w, const char *text)
{
	if (w < 2)
		return;

	const int32_t textW = (int32_t)strlen(text) * (FONT_CHAR_W-1);
	const int32_t textX = x + (((w - 2) - textW) >> 1);

	drawFramework1(x, y, w, 11);
	textOut2(textX, y+3, text);
}

void drawUpButton(int32_t x, int32_t y)
{
	drawFramework1(x, y, 11, 11);
	textOut2(x+1, y+2, ARROW_UP_STR);
}

void drawDownButton(int32_t x, int32_t y)
{
	drawFramework1(x, y, 11, 11);
	textOut2(x+1, y+2, ARROW_DOWN_STR);
}

void statusAllRight(void)
{
	setStatusMessage("ALL RIGHT", DO_CARRY);
}

void statusOutOfMemory(void)
{
	displayErrorMsg("OUT OF MEMORY !!!");
}

void statusSampleIsEmpty(void)
{
	displayErrorMsg("SAMPLE IS EMPTY");
}

void changeStatusText(const char *text)
{
	fillRect(88, 127, 17*FONT_CHAR_W, FONT_CHAR_H, video.palette[PAL_GENBKG]);
	textOut(88, 127, text, video.palette[PAL_GENTXT]);
}

void statusNotSampleZero(void)
{
	/* This rather confusing error message actually means that
	** you can't load a sample to sample slot #0 (which isn't a
	** real sample slot).
	*/
	displayErrorMsg("NOT SAMPLE 0 !");
}

void renderFrame(void)
{
	updateMOD2WAVDialog(); // must be first to avoid flickering issues

	updateSongInfo1(); // top left side of screen, when "disk op"/"pos ed" is hidden
	updateSongInfo2(); // two middle rows of screen, always visible
	updateEditOp();
	updatePatternData();
	updateDiskOp();
	updateSampler();
	updatePosEd();
	updateVisualizer();
	handleLastGUIObjectDown();
	drawSamplerLine();
	writeSampleMonitorWaveform();
}

void resetAllScreens(void) // prepare GUI for "really quit?" dialog
{
	editor.mixFlag = false;
	editor.swapChannelFlag = false;
	ui.clearScreenShown = false;

	ui.changingChordNote = false;
	ui.changingSmpResample = false;
	ui.changingSamplingNote = false;
	ui.changingDrumPadNote = false;

	ui.pat2SmpDialogShown = false;
	ui.disablePosEd = false;
	ui.disableVisualizer = false;
	
	if (ui.samplerScreenShown)
	{
		if (ui.samplingBoxShown)
		{
			ui.samplingBoxShown = false;
			removeSamplingBox();
		}

		ui.samplerVolBoxShown = false;
		ui.samplerFiltersBoxShown = false;
		displaySample(); // removes volume/filter box
	}

	if (ui.editTextFlag)
		exitGetTextLine(EDIT_TEXT_NO_UPDATE);
}

void removeAskDialog(void)
{
	if (!ui.askScreenShown && !editor.isWAVRendering)
		displayMainScreen();

	ui.disablePosEd = false;
	ui.disableVisualizer = false;
}

void renderAskDialog(void)
{
	ui.disablePosEd = true;
	ui.disableVisualizer = true;

	const uint32_t *srcPtr = ui.pat2SmpDialogShown ? pat2SmpDialogBMP : yesNoDialogBMP;
	blit32(160, 51, 104, 39, srcPtr);
}

void renderBigAskDialog(void)
{
	ui.disablePosEd = true;
	ui.disableVisualizer = true;

	blit32(120, 44, 200, 55, bigYesNoDialogBMP);
}

void showDownsampleAskDialog(void)
{
	ui.askScreenShown = true;
	ui.askScreenType = ASK_LOAD_DOWNSAMPLE;
	pointerSetMode(POINTER_MODE_MSG1, NO_CARRY);
	setStatusMessage("PLEASE SELECT", NO_CARRY);
	renderBigAskDialog();

	textOutTight(133, 49, "THE SAMPLE'S FREQUENCY IS", video.palette[PAL_BACKGRD]);
	textOutTight(154, 57, "HIGH (ABOVE 22KHZ).", video.palette[PAL_BACKGRD]);
	textOutTight(133, 65, "DO YOU WANT TO DOWNSAMPLE", video.palette[PAL_BACKGRD]);
	textOutTight(156, 73, "BEFORE LOADING IT?", video.palette[PAL_BACKGRD]);
}

static void fillFromVuMetersBgBuffer(void)
{
	const uint32_t *srcPtr;
	uint32_t *dstPtr;

	if (ui.samplerScreenShown || editor.isWAVRendering || editor.isSMPRendering)
		return;

	srcPtr = vuMetersBg;
	dstPtr = &video.frameBuffer[(187 * SCREEN_W) + 55];

	for (uint32_t i = 0; i < AMIGA_VOICES; i++)
	{
		for (uint32_t y = 0; y < 48; y++)
		{
			for (uint32_t x = 0; x < 10; x++)
				dstPtr[x] = srcPtr[x];

			srcPtr += 10;
			dstPtr -= SCREEN_W;
		}

		dstPtr += (SCREEN_W * 48) + 72;
	}
}

void fillToVuMetersBgBuffer(void)
{
	const uint32_t *srcPtr;
	uint32_t *dstPtr;

	if (ui.samplerScreenShown || editor.isWAVRendering || editor.isSMPRendering)
		return;

	srcPtr = &video.frameBuffer[(187 * SCREEN_W) + 55];
	dstPtr = vuMetersBg;

	for (uint32_t i = 0; i < AMIGA_VOICES; i++)
	{
		for (uint32_t y = 0; y < 48; y++)
		{
			for (uint32_t x = 0; x < 10; x++)
				dstPtr[x] = srcPtr[x];

			srcPtr -= SCREEN_W;
			dstPtr += 10;
		}

		srcPtr += (SCREEN_W * 48) + 72;
	}
}

void renderVuMeters(void)
{
	const uint32_t *srcPtr;
	uint32_t h, *dstPtr;

	if (ui.samplerScreenShown || editor.isWAVRendering || editor.isSMPRendering)
		return;

	fillToVuMetersBgBuffer();
	
	dstPtr = &video.frameBuffer[(187 * SCREEN_W) + 55];
	for (uint32_t i = 0; i < AMIGA_VOICES; i++)
	{
		if (config.realVuMeters)
			h = editor.realVuMeterVolumes[i];
		else
			h = editor.vuMeterVolumes[i];

		if (h > 48)
			h = 48;

		srcPtr = vuMeterBMP;
		for (uint32_t y = 0; y < h; y++)
		{
			for (uint32_t x = 0; x < 10; x++)
				dstPtr[x] = srcPtr[x];

			srcPtr += 10;
			dstPtr -= SCREEN_W;
		}

		dstPtr += (SCREEN_W * h) + 72;
	}
}

void updateSongInfo1(void) // left side of screen, when Disk Op. is hidden
{
	moduleSample_t *currSample;

	if (ui.diskOpScreenShown)
		return;

	currSample = &song->samples[editor.currSample];

	if (ui.updateSongPos)
	{
		ui.updateSongPos = false;
		printThreeDecimalsBg(72, 3, *editor.currPosDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateSongPattern)
	{
		ui.updateSongPattern = false;
		printTwoDecimalsBg(80, 14, *editor.currPatternDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateSongLength)
	{
		ui.updateSongLength = false;
		if (!editor.isWAVRendering)
			printThreeDecimalsBg(72, 25, *editor.currLengthDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateCurrSampleFineTune)
	{
		ui.updateCurrSampleFineTune = false;

		if (!editor.isWAVRendering)
			textOutBg(80, 36, ftuneStrTab[currSample->fineTune & 0xF], video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateCurrSampleNum)
	{
		ui.updateCurrSampleNum = false;
		if (!editor.isWAVRendering)
		{
			printTwoHexBg(80, 47,
				editor.sampleZero ? 0 : ((*editor.currSampleDisp) + 1), video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}

	if (ui.updateCurrSampleVolume)
	{
		ui.updateCurrSampleVolume = false;
		if (!editor.isWAVRendering)
			printTwoHexBg(80, 58, *currSample->volumeDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateCurrSampleLength)
	{
		ui.updateCurrSampleLength = false;
		if (!editor.isWAVRendering)
		{
			if (config.maxSampleLength == 0xFFFE)
				printFourHexBg(64, 69, *currSample->lengthDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				printFiveHexBg(56, 69, *currSample->lengthDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}

	if (ui.updateCurrSampleRepeat)
	{
		ui.updateCurrSampleRepeat = false;
		if (config.maxSampleLength == 0xFFFE)
			printFourHexBg(64, 80, *currSample->loopStartDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		else
			printFiveHexBg(56, 80, *currSample->loopStartDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateCurrSampleReplen)
	{
		ui.updateCurrSampleReplen = false;
		if (config.maxSampleLength == 0xFFFE)
			printFourHexBg(64, 91, *currSample->loopLengthDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		else
			printFiveHexBg(56, 91, *currSample->loopLengthDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}
}

void updateSongInfo2(void) // two middle rows of screen, always present
{
	char tempChar;
	int32_t x, i;
	moduleSample_t *currSample;

	if (ui.updateStatusText)
	{
		ui.updateStatusText = false;

		// clear background
		fillRect(88, 127, 17*FONT_CHAR_W, FONT_CHAR_H, video.palette[PAL_GENBKG]);

		// render status text
		if (!editor.errorMsgActive && editor.blockMarkFlag && !ui.askScreenShown
			&& !ui.clearScreenShown && !editor.swapChannelFlag)
		{
			textOut(88, 127, "MARK BLOCK", video.palette[PAL_GENTXT]);
			charOut(192, 127, '-', video.palette[PAL_GENTXT]);

			editor.blockToPos = song->currRow;
			if (editor.blockFromPos >= editor.blockToPos)
			{
				printTwoDecimals(176, 127, editor.blockToPos, video.palette[PAL_GENTXT]);
				printTwoDecimals(200, 127, editor.blockFromPos, video.palette[PAL_GENTXT]);
			}
			else
			{
				printTwoDecimals(176, 127, editor.blockFromPos, video.palette[PAL_GENTXT]);
				printTwoDecimals(200, 127, editor.blockToPos, video.palette[PAL_GENTXT]);
			}
		}
		else
		{
			textOut(88, 127, ui.statusMessage, video.palette[PAL_GENTXT]);
		}
	}

	if (ui.updateSongBPM)
	{
		ui.updateSongBPM = false;
		if (!ui.samplerScreenShown)
			printThreeDecimalsBg(32, 123, song->currBPM, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateCurrPattText)
	{
		ui.updateCurrPattText = false;
		if (!ui.samplerScreenShown)
			printTwoDecimalsBg(8, 127, *editor.currEditPatternDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateTrackerFlags)
	{
		ui.updateTrackerFlags = false;

		charOutBg(1, 113, ' ', video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		charOutBg(8, 113, ' ', video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);

		if (editor.autoInsFlag)
		{
			charOut(0, 113, 'I', video.palette[PAL_GENTXT]);

			// in Amiga PT, "auto insert" 9 means 0
			if (editor.autoInsSlot == 9)
				charOut(8, 113, '0', video.palette[PAL_GENTXT]);
			else
				charOut(8, 113, '1' + editor.autoInsSlot, video.palette[PAL_GENTXT]);
		}

		charOutBg(1, 102, ' ', video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		if (editor.metroFlag)
			charOut(0, 102, 'M', video.palette[PAL_GENTXT]);

		charOutBg(16, 102, ' ', video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		if (editor.multiFlag)
			charOut(16, 102, 'M', video.palette[PAL_GENTXT]);

		charOutBg(24, 102, '0' + editor.editMoveAdd, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);

		charOutBg(311, 128, ' ', video.palette[PAL_GENBKG], video.palette[PAL_GENBKG]);
		if (editor.pNoteFlag == 1)
		{
			putPixel(314, 129, video.palette[PAL_GENTXT]);
			putPixel(315, 129, video.palette[PAL_GENTXT]);
		}
		else if (editor.pNoteFlag == 2)
		{
			putPixel(314, 128, video.palette[PAL_GENTXT]);
			putPixel(315, 128, video.palette[PAL_GENTXT]);
			putPixel(314, 130, video.palette[PAL_GENTXT]);
			putPixel(315, 130, video.palette[PAL_GENTXT]);
		}
	}

	// playback timer

	const uint32_t ms1024 = editor.musicTime64 >> 32; // milliseconds (scaled from 1000 to 1024)

	uint32_t seconds = ms1024 >> 10;
	if (seconds <= 5999) // below 100 minutes (99:59 is max for the UI)
	{
		const uint32_t MI_TimeM = seconds / 60;
		const uint32_t MI_TimeS = seconds - (MI_TimeM * 60);

		// xx:xx
		printTwoDecimalsBg(272, 102, MI_TimeM, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		printTwoDecimalsBg(296, 102, MI_TimeS, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}
	else
	{
		// 99:59
		printTwoDecimalsBg(272, 102, 99, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		printTwoDecimalsBg(296, 102, 59, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateSongName)
	{
		ui.updateSongName = false;
		for (x = 0; x < 20; x++)
		{
			tempChar = song->header.name[x];
			if (tempChar == '\0')
				tempChar = '_';

			charOutBg(104 + (x * FONT_CHAR_W), 102, tempChar, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}

	if (ui.updateCurrSampleName)
	{
		ui.updateCurrSampleName = false;
		currSample = &song->samples[editor.currSample];

		for (x = 0; x < 22; x++)
		{
			tempChar = currSample->text[x];
			if (tempChar == '\0')
				tempChar = '_';

			charOutBg(104 + (x * FONT_CHAR_W), 113, tempChar, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}

	if (ui.updateSongSize)
	{
		ui.updateSongSize = false;

		// clear background
		fillRect(264, 123, 6*FONT_CHAR_W, FONT_CHAR_H, video.palette[PAL_GENBKG]);

		// calculate module length

		uint32_t totalSampleDataSize = 0;
		for (i = 0; i < MOD_SAMPLES; i++)
			totalSampleDataSize += song->samples[i].length;

		uint32_t totalPatterns = 0;
		for (i = 0; i < MOD_ORDERS; i++)
		{
			if (song->header.order[i] > totalPatterns)
				totalPatterns = song->header.order[i];
		}

		uint32_t moduleSize = 2108 + (totalPatterns * 1024) + totalSampleDataSize;
		if (moduleSize > 999999)
		{
			charOut(304, 123, 'K', video.palette[PAL_GENTXT]);
			printFourDecimals(272, 123, moduleSize / 1000, video.palette[PAL_GENTXT]);
		}
		else
		{
			printSixDecimals(264, 123, moduleSize, video.palette[PAL_GENTXT]);
		}
	}

	if (ui.updateSongTiming)
	{
		ui.updateSongTiming = false;
		textOutBg(288, 130, (editor.timingMode == TEMPO_MODE_CIA) ? "CIA" : "VBL", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}
}

void updateCursorPos(void)
{
	if (!ui.samplerScreenShown)
		setSpritePos(SPRITE_PATTERN_CURSOR, cursorPosTable[cursor.pos], 188);
}

void updateSampler(void)
{
	int32_t tmpSampleOffset;
	moduleSample_t *s;

	if (!ui.samplerScreenShown || ui.samplingBoxShown)
		return;

	assert(editor.currSample >= 0 && editor.currSample <= 30);
	s = &song->samples[editor.currSample];

	// update 9xx offset
	if (mouse.y >= 138 && mouse.y <= 201 && mouse.x >= 3 && mouse.x <= 316)
	{
		if (!ui.samplerVolBoxShown && !ui.samplerFiltersBoxShown && s->length > 0)
		{
			tmpSampleOffset = 0x900 + (scr2SmpPos(mouse.x-3) >> 8);
			if (tmpSampleOffset != ui.lastSampleOffset)
			{
				ui.lastSampleOffset = (uint16_t)tmpSampleOffset;
				ui.update9xxPos = true;
			}
		}
	}

	// display 9xx offset
	if (ui.update9xxPos)
	{
		ui.update9xxPos = false;
		if (ui.lastSampleOffset <= 0x900 || ui.lastSampleOffset > 0x9FF)
			textOutBg(288, 247, "---", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		else
			printThreeHexBg(288, 247, ui.lastSampleOffset, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
	}

	if (ui.updateResampleNote)
	{
		ui.updateResampleNote = false;

		// show resample note
		if (ui.changingSmpResample)
		{
			textOutBg(288, 236, "---", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
		else
		{
			assert(editor.resampleNote < 36);
			textOutBg(288, 236,
				config.accidental ? noteNames2[2+editor.resampleNote] : noteNames1[2+editor.resampleNote],
				video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}

	if (ui.samplerVolBoxShown)
	{
		if (ui.updateVolFromText)
		{
			ui.updateVolFromText = false;
			printThreeDecimalsBg(176, 157, *editor.vol1Disp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateVolToText)
		{
			ui.updateVolToText = false;
			printThreeDecimalsBg(176, 168, *editor.vol2Disp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}
	else if (ui.samplerFiltersBoxShown)
	{
		if (ui.updateLPText)
		{
			ui.updateLPText = false;
			printFourDecimalsBg(168, 157, *editor.lpCutOffDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateHPText)
		{
			ui.updateHPText = false;
			printFourDecimalsBg(168, 168, *editor.hpCutOffDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateNormFlag)
		{
			ui.updateNormFlag = false;

			if (editor.normalizeFiltersFlag)
				textOutBg(208, 179, "YES", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				textOutBg(208, 179, "NO ", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}
}

void showVolFromSlider(void)
{
	uint32_t *dstPtr, pixel, bgPixel, sliderStart, sliderEnd;

	sliderStart = ((editor.vol1 * 3) + 5) / 10;
	sliderEnd  = sliderStart + 4;
	pixel = video.palette[PAL_QADSCP];
	bgPixel = video.palette[PAL_BACKGRD];
	dstPtr = &video.frameBuffer[(158 * SCREEN_W) + 105];

	for (uint32_t y = 0; y < 3; y++)
	{
		for (uint32_t x = 0; x < 65; x++)
		{
			if (x >= sliderStart && x <= sliderEnd)
				dstPtr[x] = pixel;
			else
				dstPtr[x] = bgPixel;
		}

		dstPtr += SCREEN_W;
	}
}

void showVolToSlider(void)
{
	uint32_t *dstPtr, pixel, bgPixel, sliderStart, sliderEnd;

	sliderStart = ((editor.vol2 * 3) + 5) / 10;
	sliderEnd = sliderStart + 4;
	pixel = video.palette[PAL_QADSCP];
	bgPixel = video.palette[PAL_BACKGRD];
	dstPtr = &video.frameBuffer[(169 * SCREEN_W) + 105];

	for (uint32_t y = 0; y < 3; y++)
	{
		for (uint32_t x = 0; x < 65; x++)
		{
			if (x >= sliderStart && x <= sliderEnd)
				dstPtr[x] = pixel;
			else
				dstPtr[x] = bgPixel;
		}

		dstPtr += SCREEN_W;
	}
}

void renderSamplerVolBox(void)
{
	blit32(72, 154, 136, 33, samplerVolumeBMP);

	ui.updateVolFromText = true;
	ui.updateVolToText = true;
	showVolFromSlider();
	showVolToSlider();

	// hide loop sprites
	hideSprite(SPRITE_LOOP_PIN_LEFT);
	hideSprite(SPRITE_LOOP_PIN_RIGHT);
}

void removeSamplerVolBox(void)
{
	displaySample();
}

void renderSamplerFiltersBox(void)
{
	blit32(65, 154, 186, 33, samplerFiltersBMP);

	textOut(200, 157, "HZ", video.palette[PAL_GENTXT]);
	textOut(200, 168, "HZ", video.palette[PAL_GENTXT]);

	ui.updateLPText = true;
	ui.updateHPText = true;
	ui.updateNormFlag = true;

	// hide loop sprites
	hideSprite(SPRITE_LOOP_PIN_LEFT);
	hideSprite(SPRITE_LOOP_PIN_RIGHT);
}

void removeSamplerFiltersBox(void)
{
	displaySample();
}

void updatePosEd(void)
{
	if (!ui.posEdScreenShown || !ui.updatePosEd)
		return;

	ui.updatePosEd = false;

	if (!ui.disablePosEd)
	{
		int32_t posEdPosition = song->currOrder;
		if (posEdPosition > song->header.numOrders-1)
			posEdPosition = song->header.numOrders-1;

		// top five
		for (int32_t y = 0; y < 5; y++)
		{
			if (posEdPosition-(5-y) >= 0)
			{
				printThreeDecimalsBg(128, 23+(y*6), posEdPosition-(5-y), video.palette[PAL_QADSCP], video.palette[PAL_BACKGRD]);
				printTwoDecimalsBg(160, 23+(y*6), song->header.order[posEdPosition-(5-y)], video.palette[PAL_QADSCP], video.palette[PAL_BACKGRD]);
			}
			else
			{
				fillRect(128, 23+(y*6), 22*FONT_CHAR_W, FONT_CHAR_H, video.palette[PAL_BACKGRD]);
			}
		}

		// middle
		printThreeDecimalsBg(128, 53, posEdPosition, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		printTwoDecimalsBg(160, 53, *editor.currPosEdPattDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);

		// bottom six
		for (int32_t y = 0; y < 6; y++)
		{
			if (posEdPosition+y < song->header.numOrders-1)
			{
				printThreeDecimalsBg(128, 59+(y*6), posEdPosition+(y+1), video.palette[PAL_QADSCP], video.palette[PAL_BACKGRD]);
				printTwoDecimalsBg(160, 59+(y*6), song->header.order[posEdPosition+(y+1)], video.palette[PAL_QADSCP], video.palette[PAL_BACKGRD]);
			}
			else
			{
				fillRect(128, 59+(y*6), 22*FONT_CHAR_W, FONT_CHAR_H, video.palette[PAL_BACKGRD]);
			}
		}

		// kludge to fix bottom part of text edit marker in pos ed
		if (ui.editTextFlag && ui.editObject == PTB_PE_PATT)
			renderTextEditMarker();
	}
}

void renderPosEdScreen(void)
{
	blit32(120, 0, 200, 99, posEdBMP);
}

void renderMuteButtons(void)
{
	const uint32_t *srcPtr;
	uint32_t *dstPtr, srcPitch;

	if (ui.diskOpScreenShown || ui.posEdScreenShown)
		return;

	dstPtr = &video.frameBuffer[(3 * SCREEN_W) + 310];
	for (uint32_t i = 0; i < AMIGA_VOICES; i++)
	{
		if (editor.muted[i])
		{
			srcPtr = &muteButtonsBMP[i * (6 * 7)];
			srcPitch = 7;
		}
		else
		{
			srcPtr = &trackerFrameBMP[((3 + (i * 11)) * SCREEN_W) + 310];
			srcPitch = SCREEN_W;
		}

		for (uint32_t y = 0; y < 6; y++)
		{
			for (uint32_t x = 0; x < 7; x++)
				dstPtr[x] = srcPtr[x];

			srcPtr += srcPitch;
			dstPtr += SCREEN_W;
		}

		dstPtr += SCREEN_W * 5;
	}
}

void renderClearScreen(void)
{
	ui.disablePosEd = true;
	ui.disableVisualizer = true;

	blit32(160, 51, 104, 39, clearDialogBMP);
}

void removeClearScreen(void)
{
	displayMainScreen();

	ui.disablePosEd = false;
	ui.disableVisualizer = false;
}

void updateCurrSample(void)
{
	ui.updateCurrSampleName = true;
	ui.updateSongSize = true;

	if (!ui.diskOpScreenShown)
	{
		ui.updateCurrSampleFineTune = true;
		ui.updateCurrSampleNum = true;
		ui.updateCurrSampleVolume = true;
		ui.updateCurrSampleLength = true;
		ui.updateCurrSampleRepeat = true;
		ui.updateCurrSampleReplen = true;
	}

	if (ui.samplerScreenShown)
		redrawSample();

	updateSamplePos();
	recalcChordLength();

	sampler.tmpLoopStart = 0;
	sampler.tmpLoopLength = 0;
}

void updatePatternData(void)
{
	if (ui.updatePatternData)
	{
		ui.updatePatternData = false;
		if (!ui.samplerScreenShown)
			redrawPattern();
	}
}

void removeTextEditMarker(void)
{
	if (!ui.editTextFlag)
		return;

	if (ui.editObject == PTB_PE_PATT)
	{
		// position editor text editing
		hLine(ui.lineCurX - 4, ui.lineCurY - 1, 7, video.palette[PAL_GENBKG2]);

		// no need to clear the second row of pixels

		ui.updatePosEd = true;
	}
	else
	{
		// all others
		fillRect(ui.lineCurX - 4, ui.lineCurY - 1, 7, 2, video.palette[PAL_GENBKG]);
	}
}

void renderTextEditMarker(void)
{
	if (!ui.editTextFlag)
		return;

	fillRect(ui.lineCurX - 4, ui.lineCurY - 1, 7, 2, video.palette[PAL_TEXTMARK]);
}

static void sendMouseButtonUpEvent(uint8_t button)
{
	SDL_Event event;

	memset(&event, 0, sizeof (event));

	event.type = SDL_MOUSEBUTTONUP;
	event.button.button = button;

	SDL_PushEvent(&event);
}

void handleLastGUIObjectDown(void)
{
	bool testMouseButtonRelease = false;

	if (ui.sampleMarkingPos >= 0)
	{
		samplerSamplePressed(MOUSE_BUTTON_HELD);
		testMouseButtonRelease = true;
	}

	if (ui.forceSampleDrag)
	{
		samplerBarPressed(MOUSE_BUTTON_HELD);
		testMouseButtonRelease = true;
	}

	if (ui.forceSampleEdit)
	{
		samplerEditSample(MOUSE_BUTTON_HELD);
		testMouseButtonRelease = true;
	}

	if (ui.forceVolDrag)
	{
		volBoxBarPressed(MOUSE_BUTTON_HELD);
		testMouseButtonRelease = true;
	}

	/* Hack to send "mouse button up" events if we released the mouse button(s)
	** outside of the window...
	*/
	if (testMouseButtonRelease)
	{
		if (mouse.x < 0 || mouse.x >= SCREEN_W || mouse.y < 0 || mouse.y >= SCREEN_H)
		{
			if (mouse.leftButtonPressed && !(mouse.buttonState & SDL_BUTTON_LMASK))
				sendMouseButtonUpEvent(SDL_BUTTON_LEFT);

			if (mouse.rightButtonPressed && !(mouse.buttonState & SDL_BUTTON_RMASK))
				sendMouseButtonUpEvent(SDL_BUTTON_RIGHT);
		}
	}
}

void updateVisualizer(void)
{
	if (ui.disableVisualizer || ui.diskOpScreenShown ||
		ui.posEdScreenShown  || ui.editOpScreenShown ||
		ui.aboutScreenShown  || ui.askScreenShown    ||
		editor.isWAVRendering || ui.samplingBoxShown)
	{
		return;
	}

	if (ui.visualizerMode == VISUAL_SPECTRUM)
	{
		// spectrum analyzer

		uint32_t *dstPtr = &video.frameBuffer[(59 * SCREEN_W) + 129];
		for (uint32_t i = 0; i < SPECTRUM_BAR_NUM; i++)
		{
			const uint32_t *srcPtr = analyzerColorsRGB24;
			uint32_t pixel = video.palette[PAL_GENBKG];

			int32_t tmpVol = editor.spectrumVolumes[i];
			if (tmpVol > SPECTRUM_BAR_HEIGHT)
				tmpVol = SPECTRUM_BAR_HEIGHT;

			for (int32_t y = SPECTRUM_BAR_HEIGHT-1; y >= 0; y--)
			{
				if (y < tmpVol)
					pixel = srcPtr[y];

				for (uint32_t x = 0; x < SPECTRUM_BAR_WIDTH; x++)
					dstPtr[x] = pixel;

				dstPtr += SCREEN_W;
			}

			dstPtr -= (SCREEN_W * SPECTRUM_BAR_HEIGHT) - (SPECTRUM_BAR_WIDTH + 2);
		}
	}
	else
	{
		drawScopes();
	}
}

void renderQuadrascopeBg(void)
{
	const uint32_t *srcPtr;
	uint32_t *dstPtr;

	srcPtr = &trackerFrameBMP[(44 * SCREEN_W) + 120];
	dstPtr = &video.frameBuffer[(44 * SCREEN_W) + 120];

	for (uint32_t y = 0; y < 55; y++)
	{
		memcpy(dstPtr, srcPtr, 200 * sizeof (int32_t));

		srcPtr += SCREEN_W;
		dstPtr += SCREEN_W;
	}

	for (int32_t i = 0; i < AMIGA_VOICES; i++)
		scope[i].emptyScopeDrawn = false;
}

void renderSpectrumAnalyzerBg(void)
{
	blit32(120, 44, 200, 55, spectrumVisualsBMP);
}

void renderAboutScreen(void)
{
	char verString[16];
	uint32_t verStringX;

	if (!ui.aboutScreenShown || ui.diskOpScreenShown || ui.posEdScreenShown || ui.editOpScreenShown)
		return;

	blit32(120, 44, 200, 55, aboutScreenBMP);

	// draw version string

	sprintf(verString, "v%s", PROG_VER_STR);
	verStringX = 260 + (((63 - ((uint32_t)strlen(verString) * (FONT_CHAR_W - 1))) + 1) / 2);
	textOutTight(verStringX, 67, verString, video.palette[PAL_GENBKG2]);
}

void renderEditOpMode(void)
{
	const uint32_t *srcPtr;

	// select what character box to render

	switch (ui.editOpScreen)
	{
		default:
		case 0:
			srcPtr = &editOpModeCharsBMP[editor.sampleAllFlag ? EDOP_MODE_BMP_A_OFS : EDOP_MODE_BMP_S_OFS];
		break;

		case 1:
		{
			     if (editor.trackPattFlag == 0) srcPtr = &editOpModeCharsBMP[EDOP_MODE_BMP_T_OFS];
			else if (editor.trackPattFlag == 1) srcPtr = &editOpModeCharsBMP[EDOP_MODE_BMP_P_OFS];
			else srcPtr = &editOpModeCharsBMP[EDOP_MODE_BMP_S_OFS];
		}
		break;

		case 2:
			srcPtr = &editOpModeCharsBMP[editor.halfClipFlag ? EDOP_MODE_BMP_C_OFS : EDOP_MODE_BMP_H_OFS];
		break;

		case 3:
			srcPtr = (editor.newOldFlag == 0) ? &editOpModeCharsBMP[EDOP_MODE_BMP_N_OFS] : &editOpModeCharsBMP[EDOP_MODE_BMP_O_OFS];
		break;
	}

	// render it...
	blit32(310, 47, 7, 6, srcPtr);
}

void renderEditOpScreen(void)
{
	const uint32_t *srcPtr;

	// select which graphics to render
	switch (ui.editOpScreen)
	{
		default:
		case 0: srcPtr = editOpScreen1BMP; break;
		case 1: srcPtr = editOpScreen2BMP; break;
		case 2: srcPtr = editOpScreen3BMP; break;
		case 3: srcPtr = editOpScreen4BMP; break;
	}

	blit32(120, 44, 200, 55, srcPtr);

	// fix graphics in 128K sample mode
	if (config.maxSampleLength != 65534)
	{
		if (ui.editOpScreen == 2)
			blit32(213, 55, 32, 11, fix128KPosBMP);
		else if (ui.editOpScreen == 3)
			blit32(120, 88, 48, 11, fix128KChordBMP);
	}

	renderEditOpMode();

	// render text and content
	if (ui.editOpScreen == 0)
	{
		textOut(128, 47, "  TRACK      PATTERN  ", video.palette[PAL_GENTXT]);
	}
	else if (ui.editOpScreen == 1)
	{
		textOut(128, 47, "  RECORD     SAMPLES  ", video.palette[PAL_GENTXT]);

		ui.updateRecordText = true;
		ui.updateQuantizeText = true;
		ui.updateMetro1Text = true;
		ui.updateMetro2Text = true;
		ui.updateFromText = true;
		ui.updateKeysText = true;
		ui.updateToText = true;
	}
	else if (ui.editOpScreen == 2)
	{
		textOut(128, 47, "    SAMPLE EDITOR     ", video.palette[PAL_GENTXT]);
		charOut(272, 91, '%', video.palette[PAL_GENTXT]); // for Volume text

		ui.updatePosText = true;
		ui.updateModText = true;
		ui.updateVolText = true;
	}
	else if (ui.editOpScreen == 3)
	{
		textOut(128, 47, " SAMPLE CHORD EDITOR  ", video.palette[PAL_GENTXT]);

		ui.updateChordLengthText = true;
		ui.updateChordNote1Text = true;
		ui.updateChordNote2Text = true;
		ui.updateChordNote3Text = true;
		ui.updateChordNote4Text = true;
	}
}

void renderMOD2WAVDialog(void)
{
	blit32(64, 27, 192, 48, mod2wavBMP);
}

void updateMOD2WAVDialog(void)
{
	if (!ui.updateMod2WavDialog)
		return;

	ui.updateMod2WavDialog = false;

	if (editor.isWAVRendering)
	{
		if (ui.mod2WavFinished)
		{
			ui.mod2WavFinished = false;

			resetSong();
			pointerSetMode(POINTER_MODE_IDLE, DO_CARRY);

			if (editor.abortMod2Wav)
			{
				displayErrorMsg("MOD2WAV ABORTED !");
			}
			else
			{
				displayMsg("MOD RENDERED !");
				setMsgPointer();
			}

			editor.isWAVRendering = false;
			modSetTempo(song->currBPM, true); // update BPM with normal audio output rate
			displayMainScreen();
		}
		else
		{
			if (song->rowsInTotal == 0)
				return;

			// render progress bar

			int32_t percent = (song->rowsCounter * 100) / song->rowsInTotal;
			if (percent > 100)
				percent = 100;

			// foreground (progress)
			const int32_t progressBarWidth = ((percent * 180) + 50) / 100;
			if (progressBarWidth > 0)
				fillRect(70, 42, progressBarWidth, 11, video.palette[PAL_GENBKG2]); // foreground (progress)

			// background
			int32_t bgWidth = 180 - progressBarWidth;
			if (bgWidth > 0)
				fillRect(70+progressBarWidth, 42, bgWidth, 11, video.palette[PAL_BORDER]);

			// draw percentage text
			if (percent > 99)
				printThreeDecimals(144, 45, percent, video.palette[PAL_GENTXT]);
			else
				printTwoDecimals(152, 45, percent, video.palette[PAL_GENTXT]);

			charOut(168, 45, '%', video.palette[PAL_GENTXT]);
		}
	}
}

void updateEditOp(void)
{
	if (!ui.editOpScreenShown || ui.posEdScreenShown || ui.diskOpScreenShown)
		return;

	if (ui.editOpScreen == 1)
	{
		if (ui.updateRecordText)
		{
			ui.updateRecordText = false;
			textOutBg(176, 58, (editor.recordMode == RECORD_PATT) ? "PATT" : "SONG", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateQuantizeText)
		{
			ui.updateQuantizeText = false;
			printTwoDecimalsBg(192, 69, *editor.quantizeValueDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateMetro1Text)
		{
			ui.updateMetro1Text = false;
			printTwoDecimalsBg(168, 80, *editor.metroSpeedDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateMetro2Text)
		{
			ui.updateMetro2Text = false;
			printTwoDecimalsBg(192, 80, *editor.metroChannelDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateFromText)
		{
			ui.updateFromText = false;
			printTwoHexBg(264, 80, *editor.sampleFromDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateKeysText)
		{
			ui.updateKeysText = false;
			textOutBg(160, 91, editor.multiFlag ? "MULTI " : "SINGLE", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateToText)
		{
			ui.updateToText = false;
			printTwoHexBg(264, 91, *editor.sampleToDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}
	else if (ui.editOpScreen == 2)
	{
		if (ui.updateMixText)
		{
			ui.updateMixText = false;
			if (editor.mixFlag)
			{
				textOutBg(128, 47, editor.mixText, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
				textOutBg(248, 47, "  ", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			}
			else
			{
				textOutBg(128, 47, "    SAMPLE EDITOR     ", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			}
		}

		if (ui.updatePosText)
		{
			ui.updatePosText = false;
			if (config.maxSampleLength == 0xFFFE)
				printFourHexBg(248, 58, *editor.samplePosDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				printFiveHexBg(240, 58, *editor.samplePosDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateModText)
		{
			ui.updateModText = false;
			printThreeDecimalsBg(256, 69,
				(editor.modulateSpeed < 0) ? (0 - editor.modulateSpeed) : editor.modulateSpeed,
				video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);

			if (editor.modulateSpeed < 0)
				charOutBg(248, 69, '-', video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				charOutBg(248, 69, ' ', video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateVolText)
		{
			ui.updateVolText = false;
			printThreeDecimalsBg(248, 91, *editor.sampleVolDisp, video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}
	else if (ui.editOpScreen == 3)
	{
		if (ui.updateChordLengthText)
		{
			ui.updateChordLengthText = false;

			// clear background
			if (config.maxSampleLength != 65534)
				textOutBg(160, 91, "     ", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				textOutBg(168, 91, "    ", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);

			charOut(198, 91, ':', video.palette[PAL_GENBKG]);

			if (song->samples[editor.currSample].loopLength > 2 || song->samples[editor.currSample].loopStart >= 2)
			{
				textOut(168, 91, "LOOP", video.palette[PAL_GENTXT]);
			}
			else
			{
				if (config.maxSampleLength == 0xFFFE)
					printFourHex(168, 91, *editor.chordLengthDisp, video.palette[PAL_GENTXT]); // chord max length
				else
					printFiveHex(160, 91, *editor.chordLengthDisp, video.palette[PAL_GENTXT]); // chord max length

				charOut(198, 91, (editor.chordLengthMin) ? '.' : ':', video.palette[PAL_GENTXT]); // min/max flag
			}
		}

		if (ui.updateChordNote1Text)
		{
			ui.updateChordNote1Text = false;
			if (editor.note1 > 35)
				textOutBg(256, 58, "---", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				textOutBg(256, 58, config.accidental ? noteNames2[2+editor.note1] : noteNames1[2+editor.note1],
					video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateChordNote2Text)
		{
			ui.updateChordNote2Text = false;
			if (editor.note2 > 35)
				textOutBg(256, 69, "---", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				textOutBg(256, 69, config.accidental ? noteNames2[2+editor.note2] : noteNames1[2+editor.note2],
					video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}

		if (ui.updateChordNote3Text)
		{
			ui.updateChordNote3Text = false;
			if (editor.note3 > 35)
				textOutBg(256, 80, "---", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				textOutBg(256, 80, config.accidental ? noteNames2[2+editor.note3] : noteNames1[2+editor.note3],
					video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
			
		if (ui.updateChordNote4Text)
		{
			ui.updateChordNote4Text = false;
			if (editor.note4 > 35)
				textOutBg(256, 91, "---", video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
			else
				textOutBg(256, 91, config.accidental ? noteNames2[2+editor.note4] : noteNames1[2+editor.note4],
					video.palette[PAL_GENTXT], video.palette[PAL_GENBKG]);
		}
	}
}

void displayMainScreen(void)
{
	editor.blockMarkFlag = false;

	ui.updateSongName = true;
	ui.updateSongSize = true;
	ui.updateSongTiming = true;
	ui.updateTrackerFlags = true;
	ui.updateStatusText = true;

	ui.updateCurrSampleName = true;

	if (!ui.diskOpScreenShown)
	{
		ui.updateCurrSampleFineTune = true;
		ui.updateCurrSampleNum = true;
		ui.updateCurrSampleVolume = true;
		ui.updateCurrSampleLength = true;
		ui.updateCurrSampleRepeat = true;
		ui.updateCurrSampleReplen = true;
	}

	if (ui.samplerScreenShown)
	{
		if (!ui.diskOpScreenShown)
		{
			blit32(0, 0, 320, 121, trackerFrameBMP);

			if (config.maxSampleLength != 65534)
				blit32(1, 65, 62, 34, fix128KTrackerBMP); // fix for 128kB support mode
		}
	}
	else
	{
		if (!ui.diskOpScreenShown)
		{
			blit32(0, 0, 320, 255, trackerFrameBMP);

			if (config.maxSampleLength != 65534)
				blit32(1, 65, 62, 34, fix128KTrackerBMP); // fix for 128kB support mode
		}
		else
		{
			blit32(0, 121, 320, 134, &trackerFrameBMP[121 * SCREEN_W]);
		}

		ui.updateSongBPM = true;
		ui.updateCurrPattText = true;
		ui.updatePatternData = true;
	}

	if (ui.diskOpScreenShown)
	{
		renderDiskOpScreen();
	}
	else
	{
		ui.updateSongPos = true;
		ui.updateSongPattern = true;
		ui.updateSongLength = true;

		// draw zeroes that will never change (to the left of numbers)

		charOut(64,  3, '0', video.palette[PAL_GENTXT]);
		textOut(64, 14, "00", video.palette[PAL_GENTXT]);

		if (!editor.isWAVRendering)
		{
			charOut(64, 25, '0', video.palette[PAL_GENTXT]);
			textOut(64, 47, "00", video.palette[PAL_GENTXT]);
			textOut(64, 58, "00", video.palette[PAL_GENTXT]);
		}

		if (ui.posEdScreenShown)
		{
			renderPosEdScreen();
			ui.updatePosEd = true;
		}
		else
		{
			if (ui.editOpScreenShown)
			{
				renderEditOpScreen();
			}
			else
			{
				if (ui.aboutScreenShown)
				{
					renderAboutScreen();
				}
				else
				{
					     if (ui.visualizerMode == VISUAL_QUADRASCOPE) renderQuadrascopeBg();
					else if (ui.visualizerMode == VISUAL_SPECTRUM) renderSpectrumAnalyzerBg();
				}
			}

			renderMuteButtons();
		}
	}
}

static void restoreStatusAndMousePointer(void)
{
	editor.errorMsgActive = false;
	editor.errorMsgBlock = false;
	editor.errorMsgCounter = 0;
	pointerSetPreviousMode();
	setPrevStatusMessage();
}

void handleAskNo(void)
{
	ui.pat2SmpDialogShown = false;

	switch (ui.askScreenType)
	{
		case ASK_SAVEMOD_OVERWRITE:
		{
			restoreStatusAndMousePointer();
			saveModule(DONT_CHECK_IF_FILE_EXIST, GIVE_NEW_FILENAME);
		}
		break;

		case ASK_SAVESMP_OVERWRITE:
		{
			restoreStatusAndMousePointer();
			saveSample(DONT_CHECK_IF_FILE_EXIST, GIVE_NEW_FILENAME);
		}
		break;

		case ASK_LOAD_DOWNSAMPLE:
		{
			restoreStatusAndMousePointer();
			extLoadWAVOrAIFFSampleCallback(DONT_DOWNSAMPLE);
		}
		break;

		default:
			restoreStatusAndMousePointer();
		break;
	}

	removeAskDialog();
}

void handleAskYes(void)
{
	char fileName[20 + 4 + 1];
	int8_t oldSample;
	uint32_t i;
	moduleSample_t *s;

	switch (ui.askScreenType)
	{
		case ASK_DISCARD_SONG:
		{
			restoreStatusAndMousePointer();
			diskOpLoadFile2();
		}
		break;

		case ASK_DISCARD_SONG_DRAGNDROP:
		{
			restoreStatusAndMousePointer();
			loadDroppedFile2();
		}
		break;

		case ASK_RESTORE_SAMPLE:
		{
			restoreStatusAndMousePointer();
			redoSampleData(editor.currSample);
		}
		break;

		case ASK_PAT2SMP:
		{
			restoreStatusAndMousePointer();
			doPat2Smp();
		}
		break;

		case ASK_SAVE_ALL_SAMPLES:
		{
			editor.errorMsgActive = false;
			editor.errorMsgBlock = false;
			editor.errorMsgCounter = 0;

			oldSample = editor.currSample;
			for (i = 0; i < MOD_SAMPLES; i++)
			{
				editor.currSample = (int8_t)i;
				if (song->samples[i].length > 2)
					saveSample(DONT_CHECK_IF_FILE_EXIST, GIVE_NEW_FILENAME);
			}
			editor.currSample = oldSample;

			displayMsg("SAMPLES SAVED !");
			setMsgPointer();
		}
		break;

		case ASK_MAKE_CHORD:
		{
			restoreStatusAndMousePointer();
			mixChordSample();
		}
		break;

		case ASK_BOOST_ALL_SAMPLES:
		{
			restoreStatusAndMousePointer();

			for (i = 0; i < MOD_SAMPLES; i++)
				boostSample(i, true);

			if (ui.samplerScreenShown)
				redrawSample();

			updateWindowTitle(MOD_IS_MODIFIED);
		}
		break;

		case ASK_FILTER_ALL_SAMPLES:
		{
			restoreStatusAndMousePointer();

			for (i = 0; i < MOD_SAMPLES; i++)
				filterSample(i, true);

			if (ui.samplerScreenShown)
				redrawSample();

			updateWindowTitle(MOD_IS_MODIFIED);
		}
		break;

		case ASK_UPSAMPLE:
		{
			restoreStatusAndMousePointer();
			upSample();
		}
		break;

		case ASK_DOWNSAMPLE:
		{
			restoreStatusAndMousePointer();
			downSample();
		}
		break;

		case ASK_KILL_SAMPLE:
		{
			restoreStatusAndMousePointer();

			if (editor.sampleZero)
			{
				statusNotSampleZero();
				break;
			}

			turnOffVoices();
			s = &song->samples[editor.currSample];

			s->fineTune = 0;
			s->volume = 0;
			s->length = 0;
			s->loopStart = 0;
			s->loopLength = 2;

			memset(s->text, 0, sizeof (s->text));
			memset(&song->sampleData[(editor.currSample * config.maxSampleLength)], 0, config.maxSampleLength);

			editor.samplePos = 0;
			updateCurrSample();

			ui.updateSongSize = true;
			updateWindowTitle(MOD_IS_MODIFIED);
		}
		break;

		case ASK_RESAMPLE:
		{
			restoreStatusAndMousePointer();
			samplerResample();
		}
		break;

		case ASK_LOAD_DOWNSAMPLE:
		{
			// for WAV and AIFF sample loader
			restoreStatusAndMousePointer();
			extLoadWAVOrAIFFSampleCallback(DO_DOWNSAMPLE);
		}
		break;

		case ASK_MOD2WAV_OVERWRITE:
		{
			memset(fileName, 0, sizeof (fileName));

			if (song->header.name[0] != '\0')
			{
				for (i = 0; i < 20; i++)
				{
					fileName[i] = (char)tolower(song->header.name[i]);
					if (fileName[i] == '\0') break;
					sanitizeFilenameChar(&fileName[i]);
				}

				strcat(fileName, ".wav");
			}
			else
			{
				strcpy(fileName, "untitled.wav");
			}

			renderToWav(fileName, DONT_CHECK_IF_FILE_EXIST);
		}
		break;

		case ASK_MOD2WAV:
		{
			memset(fileName, 0, sizeof (fileName));

			if (song->header.name[0] != '\0')
			{
				for (i = 0; i < 20; i++)
				{
					fileName[i] = (char)tolower(song->header.name[i]);
					if (fileName[i] == '\0') break;
					sanitizeFilenameChar(&fileName[i]);
				}

				strcat(fileName, ".wav");
			}
			else
			{
				strcpy(fileName, "untitled.wav");
			}

			renderToWav(fileName, CHECK_IF_FILE_EXIST);
		}
		break;

		case ASK_QUIT:
		{
			restoreStatusAndMousePointer();
			ui.throwExit = true;
		}
		break;

		case ASK_SAVE_SAMPLE:
		{
			restoreStatusAndMousePointer();
			saveSample(CHECK_IF_FILE_EXIST, DONT_GIVE_NEW_FILENAME);
		}
		break;

		case ASK_SAVESMP_OVERWRITE:
		{
			restoreStatusAndMousePointer();
			saveSample(DONT_CHECK_IF_FILE_EXIST, DONT_GIVE_NEW_FILENAME);
		}
		break;

		case ASK_SAVE_MODULE:
		{
			restoreStatusAndMousePointer();
			saveModule(CHECK_IF_FILE_EXIST, DONT_GIVE_NEW_FILENAME);
		}
		break;

		case ASK_SAVEMOD_OVERWRITE:
		{
			restoreStatusAndMousePointer();
			saveModule(DONT_CHECK_IF_FILE_EXIST, DONT_GIVE_NEW_FILENAME);
		}
		break;

		default: break;
	}

	removeAskDialog();
}

void videoClose(void)
{
	SDL_DestroyTexture(video.texture);
	SDL_DestroyRenderer(video.renderer);
	SDL_DestroyWindow(video.window);
	free(video.frameBufferUnaligned);
}

void setupSprites(void)
{
	memset(sprites, 0, sizeof (sprites));

	sprites[SPRITE_MOUSE_POINTER].data = mousePointerBMP;
	sprites[SPRITE_MOUSE_POINTER].pixelType = SPRITE_TYPE_PALETTE;
	sprites[SPRITE_MOUSE_POINTER].colorKey = PAL_COLORKEY;
	sprites[SPRITE_MOUSE_POINTER].w = 16;
	sprites[SPRITE_MOUSE_POINTER].h = 16;
	hideSprite(SPRITE_MOUSE_POINTER);

	sprites[SPRITE_PATTERN_CURSOR].data = patternCursorBMP;
	sprites[SPRITE_PATTERN_CURSOR].pixelType = SPRITE_TYPE_RGB;
	sprites[SPRITE_PATTERN_CURSOR].colorKey = video.palette[PAL_COLORKEY];
	sprites[SPRITE_PATTERN_CURSOR].w = 11;
	sprites[SPRITE_PATTERN_CURSOR].h = 14;
	hideSprite(SPRITE_PATTERN_CURSOR);

	sprites[SPRITE_LOOP_PIN_LEFT].data = loopPinsBMP;
	sprites[SPRITE_LOOP_PIN_LEFT].pixelType = SPRITE_TYPE_RGB;
	sprites[SPRITE_LOOP_PIN_LEFT].colorKey = video.palette[PAL_COLORKEY];
	sprites[SPRITE_LOOP_PIN_LEFT].w = 4;
	sprites[SPRITE_LOOP_PIN_LEFT].h = 64;
	hideSprite(SPRITE_LOOP_PIN_LEFT);

	sprites[SPRITE_LOOP_PIN_RIGHT].data = &loopPinsBMP[4 * 64];
	sprites[SPRITE_LOOP_PIN_RIGHT].pixelType = SPRITE_TYPE_RGB;
	sprites[SPRITE_LOOP_PIN_RIGHT].colorKey = video.palette[PAL_COLORKEY];
	sprites[SPRITE_LOOP_PIN_RIGHT].w = 4;
	sprites[SPRITE_LOOP_PIN_RIGHT].h = 64;
	hideSprite(SPRITE_LOOP_PIN_RIGHT);

	sprites[SPRITE_SAMPLING_POS_LINE].data = samplingPosBMP;
	sprites[SPRITE_SAMPLING_POS_LINE].pixelType = SPRITE_TYPE_RGB;
	sprites[SPRITE_SAMPLING_POS_LINE].colorKey = video.palette[PAL_COLORKEY];
	sprites[SPRITE_SAMPLING_POS_LINE].w = 1;
	sprites[SPRITE_SAMPLING_POS_LINE].h = 64;
	hideSprite(SPRITE_SAMPLING_POS_LINE);

	// setup refresh buffer (used to clear sprites after each frame)
	for (uint32_t i = 0; i < SPRITE_NUM; i++)
		sprites[i].refreshBuffer = (uint32_t *)malloc((sprites[i].w * sprites[i].h) * sizeof (int32_t));
}

void freeSprites(void)
{
	for (int32_t i = 0; i < SPRITE_NUM; i++)
		free(sprites[i].refreshBuffer);
}

void setSpritePos(int32_t sprite, int32_t x, int32_t y)
{
	sprites[sprite].newX = (int16_t)x;
	sprites[sprite].newY = (int16_t)y;
}

void hideSprite(int32_t sprite)
{
	sprites[sprite].newX = SCREEN_W;
}

void eraseSprites(void)
{
	int32_t sx, sy, x, y, sw, sh, srcPitch, dstPitch;
	const uint32_t *src32;
	uint32_t *dst32;
	sprite_t *s;

	for (int32_t i = SPRITE_NUM-1; i >= 0; i--) // erasing must be done in reverse order
	{
		s = &sprites[i];
		if (s->x >= SCREEN_W || s->y >= SCREEN_H) // sprite is hidden, don't draw nor fill clear buffer
			continue;

		assert(s->refreshBuffer != NULL);

		sw = s->w;
		sh = s->h;
		sx = s->x;
		sy = s->y;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			sx = 0;
		}

		// if y is negative, adjust variables
		if (sy < 0)
		{
			sh += sy; // subtraction
			sy = 0;
		}

		dst32 = &video.frameBuffer[(sy * SCREEN_W) + sx];
		src32 = s->refreshBuffer;

		// handle x/y clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;
		if (sy+sh >= SCREEN_H) sh = SCREEN_H - sy;

		srcPitch = s->w - sw;
		dstPitch = SCREEN_W - sw;

		for (y = 0; y < sh; y++)
		{
			for (x = 0; x < sw; x++)
				*dst32++ = *src32++;

			src32 += srcPitch;
			dst32 += dstPitch;
		}
	}

	fillFromVuMetersBgBuffer(); // let's put it here even though it's not sprite-based
}

void renderSprites(void)
{
	const uint8_t *src8;
	int32_t sx, sy, x, y, srcPtrBias, sw, sh, srcPitch, dstPitch;
	const uint32_t *src32;
	uint32_t *dst32, *clr32, colorKey;
	sprite_t *s;

	renderVuMeters(); // let's put it here even though it's not sprite-based

	for (int32_t i = 0; i < SPRITE_NUM; i++)
	{
		s = &sprites[i];

		// set new sprite position
		s->x = s->newX;
		s->y = s->newY;

		if (s->x >= SCREEN_W || s->y >= SCREEN_H) // sprite is hidden, don't draw nor fill clear buffer
			continue;

		assert(s->data != NULL && s->refreshBuffer != NULL);

		sw = s->w;
		sh = s->h;
		sx = s->x;
		sy = s->y;
		srcPtrBias = 0;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			srcPtrBias += -sx;
			sx = 0;
		}

		// if y is negative, adjust variables
		if (sy < 0)
		{
			sh += sy; // subtraction
			srcPtrBias += (-sy * s->w);
			sy = 0;
		}

		dst32 = &video.frameBuffer[(sy * SCREEN_W) + sx];
		clr32 = s->refreshBuffer;

		// handle x/y clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;
		if (sy+sh >= SCREEN_H) sh = SCREEN_H - sy;

		srcPitch = s->w - sw;
		dstPitch = SCREEN_W - sw;

		colorKey = sprites[i].colorKey;
		if (sprites[i].pixelType == SPRITE_TYPE_RGB)
		{
			// 24-bit RGB sprite
			src32 = ((uint32_t *)sprites[i].data) + srcPtrBias;
			for (y = 0; y < sh; y++)
			{
				for (x = 0; x < sw; x++)
				{
					*clr32++ = *dst32; // fill clear buffer
					if (*src32 != colorKey)
						*dst32 = *src32;

					dst32++;
					src32++;
				}

				clr32 += srcPitch;
				src32 += srcPitch;
				dst32 += dstPitch;
			}
		}
		else
		{
			// 8-bit paletted sprite
			src8 = ((uint8_t *)sprites[i].data) + srcPtrBias;
			for (y = 0; y < sh; y++)
			{
				for (x = 0; x < sw; x++)
				{
					*clr32++ = *dst32; // fill clear buffer
					if (*src8 != colorKey)
					{
						assert(*src8 < PALETTE_NUM);
						*dst32 = video.palette[*src8];
					}

					dst32++;
					src8++;
				}

				clr32 += srcPitch;
				src8 += srcPitch;
				dst32 += dstPitch;
			}
		}
	}
}

void flipFrame(void)
{
	const uint32_t windowFlags = SDL_GetWindowFlags(video.window);
	bool minimized = (windowFlags & SDL_WINDOW_MINIMIZED) ? true : false;

	renderSprites();
	SDL_UpdateTexture(video.texture, NULL, video.frameBuffer, SCREEN_W * sizeof (int32_t));

	// SDL 2.0.14 bug on Windows (?): This function consumes ever-increasing memory if the program is minimized
	if (!minimized)
		SDL_RenderClear(video.renderer);

	SDL_RenderCopy(video.renderer, video.texture, NULL, NULL);
	SDL_RenderPresent(video.renderer);
	eraseSprites();

	if (!video.vsync60HzPresent)
	{
		// we have no VSync, do crude thread sleeping to sync to ~60Hz
		hpc_Wait(&video.vblankHpc);
	}
	else
	{
		/* We have VSync, but it can unexpectedly get inactive in certain scenarios.
		** We have to force thread sleeping (to ~60Hz) if so.
		*/
#ifdef __APPLE__
		// macOS: VSync gets disabled if the window is 100% covered by another window. Let's add a (crude) fix:
		if (minimized || !(windowFlags & SDL_WINDOW_INPUT_FOCUS))
			hpc_Wait(&video.vblankHpc);
#elif __unix__
		// *NIX: VSync gets disabled in fullscreen mode (at least on some distros/systems). Let's add a fix:
		if (minimized || video.fullscreen)
			hpc_Wait(&video.vblankHpc);
#else
		if (minimized)
			hpc_Wait(&video.vblankHpc);
#endif
	}
}

void updateSpectrumAnalyzer(int8_t vol, int16_t period)
{
	const uint8_t maxHeight = SPECTRUM_BAR_HEIGHT + 1; // +1 because of audio latency - allows full height to be seen
	uint8_t scaledVol;
	uint32_t scaledNote;

	if (ui.visualizerMode != VISUAL_SPECTRUM || vol <= 0)
		return;

	scaledVol = (vol * 24600L) >> 16; // scaledVol = (vol * 256) / 682   = (x / 2.66)

	period = CLAMP(period, 113, 856);
	period -= 113;

	scaledNote = 743 - period;
	scaledNote *= scaledNote;
	scaledNote /= 25093; // scaledNote now ranges 0..22, no need to clamp

	// increment main spectrum bar
	editor.spectrumVolumes[scaledNote] += scaledVol;
	if (editor.spectrumVolumes[scaledNote] > maxHeight)
		editor.spectrumVolumes[scaledNote] = maxHeight;

	// increment left side of spectrum bar with half volume
	if (scaledNote > 0)
	{
		editor.spectrumVolumes[scaledNote-1] += scaledVol >> 1;
		if (editor.spectrumVolumes[scaledNote-1] > maxHeight)
			editor.spectrumVolumes[scaledNote-1] = maxHeight;
	}

	// increment right side of spectrum bar with half volume
	if (scaledNote < SPECTRUM_BAR_NUM-1)
	{
		editor.spectrumVolumes[scaledNote+1] += scaledVol >> 1;
		if (editor.spectrumVolumes[scaledNote+1] > maxHeight)
			editor.spectrumVolumes[scaledNote+1] = maxHeight;
	}
}

void sinkVisualizerBars(void)
{
	int32_t i;

	// sink stuff @ 49.92Hz (Amiga PAL) rate

	static uint64_t counter50Hz;
	const uint64_t counter50HzDelta = (uint64_t)(((UINT32_MAX+1.0) * (AMIGA_PAL_VBLANK_HZ / (double)VBLANK_HZ)) + 0.5);

	counter50Hz += counter50HzDelta; // 32.32 fixed-point counter
	if (counter50Hz > 0xFFFFFFFF)
	{
		counter50Hz &= 0xFFFFFFFF;

		// sink VU-meters
		for (i = 0; i < AMIGA_VOICES; i++)
		{
			if (editor.vuMeterVolumes[i] > 0)
				editor.vuMeterVolumes[i]--;
		}

		// sink "spectrum analyzer" bars
		for (i = 0; i < SPECTRUM_BAR_NUM; i++)
		{
			if (editor.spectrumVolumes[i] > 0)
				editor.spectrumVolumes[i]--;
		}
	}
}

void updateRenderSizeVars(void)
{
	int32_t di;
#ifdef __APPLE__
	int32_t actualScreenW, actualScreenH;
	double dXUpscale, dYUpscale;
#endif
	float fXScale, fYScale;
	SDL_DisplayMode dm;

	di = SDL_GetWindowDisplayIndex(video.window);
	if (di < 0)
		di = 0; // return display index 0 (default) on error

	SDL_GetDesktopDisplayMode(di, &dm);
	video.displayW = dm.w;
	video.displayH = dm.h;

	if (video.fullscreen)
	{
		if (config.fullScreenStretch)
		{
			video.renderW = video.displayW;
			video.renderH = video.displayH;
			video.renderX = 0;
			video.renderY = 0;
		}
		else
		{
			SDL_RenderGetScale(video.renderer, &fXScale, &fYScale);

			video.renderW = (int32_t)(SCREEN_W * fXScale);
			video.renderH = (int32_t)(SCREEN_H * fYScale);

#ifdef __APPLE__
			// retina high-DPI hackery (SDL2 is bad at reporting actual rendering sizes on macOS w/ high-DPI)
			SDL_GL_GetDrawableSize(video.window, &actualScreenW, &actualScreenH);
			SDL_GetDesktopDisplayMode(0, &dm);

			dXUpscale = (double)actualScreenW / video.displayW;
			dYUpscale = (double)actualScreenH / video.displayH;

			// downscale back to correct sizes
			if (dXUpscale != 0.0) video.renderW = (int32_t)(video.renderW / dXUpscale);
			if (dYUpscale != 0.0) video.renderH = (int32_t)(video.renderH / dYUpscale);
#endif
			video.renderX = (video.displayW - video.renderW) >> 1;
			video.renderY = (video.displayH - video.renderH) >> 1;
		}
	}
	else
	{
		SDL_GetWindowSize(video.window, &video.renderW, &video.renderH);

		video.renderX = 0;
		video.renderY = 0;
	}

	// for mouse cursor creation
	video.xScale = (int32_t)((video.renderW * (1.0 / SCREEN_W)) + 0.5);
	video.yScale = (int32_t)((video.renderH * (1.0 / SCREEN_H)) + 0.5);
	createMouseCursors();
}

void toggleFullScreen(void)
{
	SDL_DisplayMode dm;

	video.fullscreen ^= 1;
	if (video.fullscreen)
	{
		if (config.fullScreenStretch)
		{
			SDL_GetDesktopDisplayMode(0, &dm);
			SDL_RenderSetLogicalSize(video.renderer, dm.w, dm.h);
		}
		else
		{
			SDL_RenderSetLogicalSize(video.renderer, SCREEN_W, SCREEN_H);
		}

		SDL_SetWindowSize(video.window, SCREEN_W, SCREEN_H);
		SDL_SetWindowFullscreen(video.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_SetWindowGrab(video.window, SDL_TRUE);
	}
	else
	{
		SDL_SetWindowFullscreen(video.window, 0);
		SDL_RenderSetLogicalSize(video.renderer, SCREEN_W, SCREEN_H);
		SDL_SetWindowSize(video.window, SCREEN_W * config.videoScaleFactor, SCREEN_H * config.videoScaleFactor);

		// this is not sensible on a multi-monitor setup
		//SDL_SetWindowPosition(video.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		SDL_SetWindowGrab(video.window, SDL_FALSE);
	}

	updateRenderSizeVars();
	updateMouseScaling();

	if (video.fullscreen)
	{
		mouse.setPosX = video.displayW >> 1;
		mouse.setPosY = video.displayH >> 1;
	}
	else
	{
		mouse.setPosX = video.renderW >> 1;
		mouse.setPosY = video.renderH >> 1;
	}

	mouse.setPosFlag = true;
}

bool setupVideo(void)
{
	int32_t screenW, screenH;
	uint32_t rendererFlags;
	SDL_DisplayMode dm;

	screenW = SCREEN_W * config.videoScaleFactor;
	screenH = SCREEN_H * config.videoScaleFactor;

	rendererFlags = 0;

#ifdef _WIN32
#if SDL_PATCHLEVEL >= 4
	SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1"); // this is for Windows only
#endif
#endif

	video.vsync60HzPresent = false;
	if (!config.vsyncOff)
	{
		SDL_GetDesktopDisplayMode(0, &dm);
		if (dm.refresh_rate >= 59 && dm.refresh_rate <= 61)
		{
			video.vsync60HzPresent = true;
			rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
		}
	}

	uint32_t windowFlags = SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI;

	video.window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, screenW, screenH, windowFlags);

	if (video.window == NULL)
	{
		showErrorMsgBox("Couldn't create SDL window:\n%s", SDL_GetError());
		return false;
	}

	video.renderer = SDL_CreateRenderer(video.window, -1, rendererFlags);
	if (video.renderer == NULL)
	{
		if (video.vsync60HzPresent) // try again without vsync flag
		{
			video.vsync60HzPresent = false;
			rendererFlags &= ~SDL_RENDERER_PRESENTVSYNC;
			video.renderer = SDL_CreateRenderer(video.window, -1, rendererFlags);
		}

		if (video.renderer == NULL)
		{
			showErrorMsgBox("Couldn't create SDL renderer:\n%s\n\n" \
			                "Is your GPU (+ driver) too old?", SDL_GetError());
			return false;
		}
	}

	SDL_RenderSetLogicalSize(video.renderer, SCREEN_W, SCREEN_H);

#if SDL_PATCHLEVEL >= 5
	SDL_RenderSetIntegerScale(video.renderer, config.integerScaling ? SDL_TRUE : SDL_FALSE);
#endif

	SDL_SetRenderDrawBlendMode(video.renderer, SDL_BLENDMODE_NONE);

	if (config.pixelFilter == PIXELFILTER_LINEAR)
		SDL_SetHint("SDL_RENDER_SCALE_QUALITY", "linear");
	else if (config.pixelFilter == PIXELFILTER_BEST)
		SDL_SetHint("SDL_RENDER_SCALE_QUALITY", "best");
	else
		SDL_SetHint("SDL_RENDER_SCALE_QUALITY", "nearest");

	video.texture = SDL_CreateTexture(video.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
	if (video.texture == NULL)
	{
		showErrorMsgBox("Couldn't create %dx%d GPU texture:\n%s\n\n" \
		                "Is your GPU (+ driver) too old?", SCREEN_W, SCREEN_H, SDL_GetError());
		return false;
	}

	SDL_SetTextureBlendMode(video.texture, SDL_BLENDMODE_NONE);

	// frame buffer used by SDL (for texture)
	video.frameBufferUnaligned = (uint32_t *)MALLOC_PAD(SCREEN_W * SCREEN_H * sizeof (int32_t), 256);
	if (video.frameBufferUnaligned == NULL)
	{
		showErrorMsgBox("Out of memory!");
		return false;
	}

	// we want an aligned pointer
	video.frameBuffer = (uint32_t *)ALIGN_PTR(video.frameBufferUnaligned, 256);

	updateRenderSizeVars();
	updateMouseScaling();

	if (config.hwMouse)
		SDL_ShowCursor(SDL_TRUE);
	else
		SDL_ShowCursor(SDL_FALSE);

	// Workaround: SDL_GetGlobalMouseState() doesn't work with KMSDRM/Wayland
	video.useDesktopMouseCoords = true;
	const char *videoDriver = SDL_GetCurrentVideoDriver();
	if (videoDriver != NULL && (strcmp("KMSDRM", videoDriver) == 0 || strcmp("wayland", videoDriver) == 0))
		video.useDesktopMouseCoords = false;

	SDL_SetRenderDrawColor(video.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	return true;
}
