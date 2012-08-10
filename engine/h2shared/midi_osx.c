/*
 * $Id$
 * based on an SDL_mixer code:
 * native_midi_macosx:  Native Midi support on Mac OS X for the SDL_mixer library
 * Copyright (C) 2009  Ryan C. Gordon <icculus@icculus.org>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Adaptation to uHexen2:
 * Copyright (C) 2012  O. Sezer <sezero@users.sourceforge.net>
 *
 * Only for Mac OS X using Core MIDI and requiring version
 * 10.3 or later. Use QuickTime with midi_mac.c, otherwise.
 */

#include "quakedef.h"
#include "bgmusic.h"
#include "midi_drv.h"

#include <CoreServices/CoreServices.h>		/* ComponentDescription */
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AvailabilityMacros.h>

/* prototypes of functions exported to BGM: */
static void *MIDI_Play (const char *filename);
static void MIDI_Update (void **handle);
static void MIDI_Rewind (void **handle);
static void MIDI_Stop (void **handle);
static void MIDI_Pause (void **handle);
static void MIDI_Resume (void **handle);
static void MIDI_SetVolume (void **handle, float value);

static midi_driver_t midi_mac_core =
{
	false, /* init success */
	"Core MIDI for Mac OS X",
	MIDI_Init,
	MIDI_Cleanup,
	MIDI_Play,
	MIDI_Update,
	MIDI_Rewind,
	MIDI_Stop,
	MIDI_Pause,
	MIDI_Resume,
	MIDI_SetVolume,
	NULL
};

typedef struct _CoreMidiSong
{
	MusicPlayer	player;
	MusicSequence	sequence;
	MusicTimeStamp	endTime;
	MusicTimeStamp	pauseTime; /* needed??? (for pause & resume??) */
	AudioUnit	audiounit;
} CoreMidiSong;

static CoreMidiSong	*currentsong = NULL;
static qboolean	midi_paused;

#define CHECK_MIDI_ALIVE()		\
do {					\
	if (!currentsong)		\
	{				\
		if (handle)		\
			*handle = NULL;	\
		return;			\
	}				\
} while (0)

static void MIDI_SetVolume (void **handle, float value)
{
	CHECK_MIDI_ALIVE();

	if (currentsong->audiounit)
	{
		AudioUnitSetParameter(currentsong->audiounit, kHALOutputParam_Volume,
				      kAudioUnitScope_Global, 0, value, 0);
	}
}

static void MIDI_Rewind (void **handle)
{
	CHECK_MIDI_ALIVE();
	MusicPlayerSetTime(currentsong->player, 0);
}

static void MIDI_Update (void **handle)
{
	MusicTimeStamp currentTime;
	CHECK_MIDI_ALIVE();

	currentTime = 0;
	MusicPlayerGetTime(currentsong->player, &currentTime);
	if (currentTime < currentsong->endTime ||
	    currentTime >= kMusicTimeStamp_EndOfTrack)
	{
		return;
	}
	if (bgmloop)
	{
		MusicPlayerSetTime(currentsong->player, 0);
	}
	else
	{
		MIDI_Stop(NULL);
		if (handle)
			*handle = NULL;
	}
}

qboolean MIDI_Init(void)
{
	if (midi_mac_core.available)
		return true;

	BGM_RegisterMidiDRV(&midi_mac_core);

	if (safemode || COM_CheckParm("-nomidi"))
		return false;

	midi_mac_core.available = true;		/* always available. */
	return true;
}

static OSStatus GetSequenceLength(MusicSequence sequence, MusicTimeStamp *_sequenceLength)
{
/* http://lists.apple.com/archives/Coreaudio-api/2003/Jul/msg00370.html
 * figure out sequence length  */
	UInt32 ntracks, i;
	MusicTimeStamp sequenceLength = 0;
	OSStatus err;

	err = MusicSequenceGetTrackCount(sequence, &ntracks);
	if (err != noErr)
		return err;

	for (i = 0; i < ntracks; ++i)
	{
		MusicTrack track;
		MusicTimeStamp tracklen = 0;
		UInt32 tracklenlen = sizeof (tracklen);

		err = MusicSequenceGetIndTrack(sequence, i, &track);
		if (err != noErr)
			return err;

		err = MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength,
					    &tracklen, &tracklenlen);
		if (err != noErr)
			return err;

		if (sequenceLength < tracklen)
			sequenceLength = tracklen;
	}

	*_sequenceLength = sequenceLength;

	return noErr;
}

/* we're looking for the sequence output audiounit. */
static OSStatus GetSequenceAudioUnit(MusicSequence sequence, AudioUnit *aunit)
{
	AUGraph graph;
	UInt32 nodecount, i;
	OSStatus err;

	err = MusicSequenceGetAUGraph(sequence, &graph);
	if (err != noErr)
		return err;

	err = AUGraphGetNodeCount(graph, &nodecount);
	if (err != noErr)
		return err;

	for (i = 0; i < nodecount; i++)
	{
		AUNode node;

		if (AUGraphGetIndNode(graph, i, &node) != noErr)
			continue;
#if (MAC_OS_X_VERSION_MIN_REQUIRED < 1050)
		/* this is deprecated, but works back to 10.0 */
		{
		struct ComponentDescription desc;
		UInt32 classdatasize = 0;
		void *classdata = NULL;
		err = AUGraphGetNodeInfo(graph, node, &desc, &classdatasize,
					 &classdata, aunit);
		if (err != noErr)
			continue;
		else if (desc.componentType != kAudioUnitType_Output)
			continue;
		else if (desc.componentSubType != kAudioUnitSubType_DefaultOutput)
			continue;
		}
#else	/* this requires 10.5 or later */
		{
		#if !defined(AUDIO_UNIT_VERSION) || ((AUDIO_UNIT_VERSION + 0) < 1060)
		/* AUGraphAddNode () is changed to take an AudioComponentDescription*
		 * desc parameter instead of a ComponentDescription* in the 10.6 SDK.
		 * AudioComponentDescription is in 10.6 or newer, but it is actually
		 * the same as struct ComponentDescription with 20 bytes of size and
		 * the same offsets of all members, therefore, is binary compatible. */
		#  define AudioComponentDescription ComponentDescription
		#endif
		AudioComponentDescription desc;
		if (AUGraphNodeInfo(graph, node, &desc, aunit) != noErr)
			continue;
		else if (desc.componentType != kAudioUnitType_Output)
			continue;
		else if (desc.componentSubType != kAudioUnitSubType_DefaultOutput)
			continue;
		}
#endif
		return noErr;	/* found it! */
	}

	return kAUGraphErr_NodeNotFound;
}

static void *MIDI_Play (const char *filename)
{
	byte *buf;
	size_t len;
	CoreMidiSong *song = NULL;
	CFDataRef data = NULL;

	if (!midi_mac_core.available)
		return NULL;

	if (!filename || !*filename)
	{
		Con_DPrintf("null music file name\n");
		return NULL;
	}

	buf = FS_LoadTempFile (filename, NULL);
	if (!buf)
	{
		Con_DPrintf("Couldn't open %s\n", filename);
		return NULL;
	}
	len = fs_filesize;

	song = (CoreMidiSong *) malloc(sizeof(CoreMidiSong));
	if (song == NULL)
		goto fail;
	memset(song, 0, sizeof(*song));

	if (NewMusicPlayer(&song->player) != noErr)
		goto fail;
	if (NewMusicSequence(&song->sequence) != noErr)
		goto fail;

	data = CFDataCreate(NULL, (const UInt8 *) buf, len);
	if (data == NULL)
		goto fail;

#if (MAC_OS_X_VERSION_MIN_REQUIRED < 1050)
	/* MusicSequenceLoadSMFData() (avail. in 10.2, no 64 bit) is
	 * equivalent to calling MusicSequenceLoadSMFDataWithFlags()
	 * with a flags value of 0 (avail. in 10.3, avail. 64 bit).
	 * So, we use MusicSequenceLoadSMFData() for powerpc versions
	 * but the *WithFlags() on intel which require 10.4 anyway. */
	#if defined(__ppc__) || defined(__POWERPC__)
	if (MusicSequenceLoadSMFData(song->sequence, data) != noErr)
		goto fail;
	#else
	if (MusicSequenceLoadSMFDataWithFlags(song->sequence, data, 0) != noErr)
		goto fail;
	#endif
#else
	/* MusicSequenceFileLoadData() requires 10.5 or later.  */
	if (MusicSequenceFileLoadData(song->sequence, data, 0, 0) != noErr)
		goto fail;
#endif

	CFRelease(data);
	data = NULL;

	if (GetSequenceLength(song->sequence, &song->endTime) != noErr)
		goto fail;
	if (MusicPlayerSetSequence(song->player, song->sequence) != noErr)
		goto fail;

	MusicPlayerPreroll(song->player);
	MusicPlayerSetTime(song->player, 0);
	MusicPlayerStart(song->player);

	currentsong = song;
	midi_paused = false;

	GetSequenceAudioUnit(song->sequence, &song->audiounit);
	if (currentsong->audiounit)
	{
		AudioUnitSetParameter(currentsong->audiounit, kHALOutputParam_Volume,
				      kAudioUnitScope_Global, 0, bgmvolume.value, 0);
	}

	return song;

fail:
	currentsong = NULL;
	if (song) {
		if (song->sequence)
			DisposeMusicSequence(song->sequence);
		if (song->player)
			DisposeMusicPlayer(song->player);
		free(song);
	}
	if (data)
		CFRelease(data);

	return NULL;
}

static void MIDI_Pause (void **handle)
{
	CHECK_MIDI_ALIVE();
	if (!midi_paused)
	{
		MusicPlayerGetTime(currentsong->player, &currentsong->pauseTime); /* needed??? */
		MusicPlayerStop(currentsong->player);
		midi_paused = true;
	}
}

static void MIDI_Resume (void **handle)
{
	CHECK_MIDI_ALIVE();
	if (midi_paused)
	{
		MusicPlayerSetTime(currentsong->player, currentsong->pauseTime); /* needed??? */
		MusicPlayerStart(currentsong->player);
		midi_paused = false;
	}
}

static void MIDI_Stop (void **handle)
{
	CHECK_MIDI_ALIVE();

	MusicPlayerStop(currentsong->player);
	DisposeMusicSequence(currentsong->sequence);
	DisposeMusicPlayer(currentsong->player);
	free(currentsong);
	currentsong = NULL;
}

void MIDI_Cleanup(void)
{
	if (midi_mac_core.available)
	{
		Con_Printf("%s: closing Core MIDI.\n", __thisfunc__);
		MIDI_Stop (NULL);
		midi_mac_core.available = false;
	}
}

