/*
	midi_sdl.c
	midiplay via SDL_mixer

	$Id: midi_sdl.c,v 1.38 2007-04-05 08:07:00 sezero Exp $
*/

#include "quakedef.h"
#include <unistd.h>
#include <dlfcn.h>
#include "sdl_inc.h"
#define _SND_SYS_MACROS_ONLY
#include "snd_sys.h"

static Mix_Music *music = NULL;
static int audio_wasinit = 0;

static qboolean bMidiInited, bFileOpen, bPlaying, bPaused, bLooped;
extern cvar_t bgmvolume;
static float bgm_volume_old = -1.0f;

static void (*midi_endmusicfnc)(void);

static void MIDI_Play_f (void)
{
	if (Cmd_Argc () == 2)
	{
		MIDI_Play(Cmd_Argv(1));
	}
}

static void MIDI_Stop_f (void)
{
	MIDI_Stop();
}

static void MIDI_Pause_f (void)
{
	MIDI_Pause (MIDI_TOGGLE_PAUSE);
}

static void MIDI_Loop_f (void)
{
	if (Cmd_Argc () == 2)
	{
		if (Q_strcasecmp(Cmd_Argv(1),"on") == 0 || Q_strcasecmp(Cmd_Argv(1),"1") == 0)
			MIDI_Loop(1);
		else if (Q_strcasecmp(Cmd_Argv(1),"off") == 0 || Q_strcasecmp(Cmd_Argv(1),"0") == 0)
			MIDI_Loop(0);
		else if (Q_strcasecmp(Cmd_Argv(1),"toggle") == 0)
			MIDI_Loop(2);
	}

	if (bLooped)
		Con_Printf("MIDI music will be looped\n");
	else
		Con_Printf("MIDI music will not be looped\n");
}

static void MIDI_SetVolume(float volume_frac)
{
	if (!bMidiInited)
		return;

	volume_frac = (volume_frac >= 0.0f) ? volume_frac : 0.0f;
	volume_frac = (volume_frac <= 1.0f) ? volume_frac : 1.0f;
	Mix_VolumeMusic(volume_frac*128); /* needs to be between 0 and 128 */
}

void MIDI_Update(void)
{
	if (bgmvolume.value != bgm_volume_old)
	{
		bgm_volume_old = bgmvolume.value;
		MIDI_SetVolume(bgm_volume_old);
	}
}

static void MIDI_EndMusicFinished(void)
{
	Sys_DPrintf("Music finished\n");

	if (bLooped)
	{
		if (Mix_PlayingMusic())
			Mix_HaltMusic();

		Sys_DPrintf("Playing again\n");
		Mix_RewindMusic();
		Mix_FadeInMusic(music,0,2000);
		bPlaying = 1;
	}
}

qboolean MIDI_Init(void)
{
	int audio_rate = 22050;
	int audio_format = AUDIO_S16SYS;
	int audio_channels = 2;
	int audio_buffers = 4096;

	void	*selfsyms;
	const SDL_version *smixer_version;
	SDL_version *(*Mix_Linked_Version_fp)(void) = NULL;

	bMidiInited = 0;
	Con_Printf("%s: ", __FUNCTION__);

	if (safemode || COM_CheckParm("-nomidi") || COM_CheckParm("-nosound") || COM_CheckParm("-s"))
	{
		Con_Printf("disabled by commandline\n");
		return 0;
	}

	if (snd_system == S_SYS_SDL)
	{
		Con_Printf("SDL_mixer conflicts SDL audio.\n");
		return 0;
	}

	Con_Printf("SDL_Mixer ");
	// this is to avoid relocation errors with very old SDL_Mixer versions
	selfsyms = dlopen(NULL, RTLD_LAZY);
	if (selfsyms != NULL)
	{
		Mix_Linked_Version_fp = dlsym(selfsyms, "Mix_Linked_Version");
		dlclose(selfsyms);
	}
	if (Mix_Linked_Version_fp == NULL)
	{
		Con_Printf("version can't be determined, disabled.\n");
		goto bad_version;
	}

	smixer_version = Mix_Linked_Version();
	Con_Printf("v%d.%d.%d is ",smixer_version->major,smixer_version->minor,smixer_version->patch);
	// reject running with SDL_Mixer versions older than what is stated in sdl_inc.h
	if (SDL_VERSIONNUM(smixer_version->major,smixer_version->minor,smixer_version->patch) < MIX_REQUIREDVERSION)
	{
		Con_Printf("too old, disabled.\n");
bad_version:
		Con_Printf("You need at least v%d.%d.%d of SDL_Mixer\n",SDL_MIXER_MIN_X,SDL_MIXER_MIN_Y,SDL_MIXER_MIN_Z);
		bMidiInited = 0;
		return 0;
	}
	Con_Printf("found.\n");

	// Try initing the audio subsys if it hasn't been already
	audio_wasinit = SDL_WasInit(SDL_INIT_AUDIO);
	if (audio_wasinit == 0)
	{
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
		{
			Con_Printf("%s: Cannot initialize SDL_AUDIO: %s\n", __FUNCTION__, SDL_GetError());
			bMidiInited = 0;
			return 0;
		}
	}

	if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) < 0)
	{
		bMidiInited = 0;
		Con_Printf("SDL_mixer: open audio failed: %s\n", SDL_GetError());
		if (audio_wasinit == 0)
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 0;
	}

	midi_endmusicfnc = &MIDI_EndMusicFinished;
	if (midi_endmusicfnc)
		Mix_HookMusicFinished(midi_endmusicfnc);

	Con_Printf("MIDI music initialized.\n");

	Cmd_AddCommand ("midi_play", MIDI_Play_f);
	Cmd_AddCommand ("midi_stop", MIDI_Stop_f);
	Cmd_AddCommand ("midi_pause", MIDI_Pause_f);
	Cmd_AddCommand ("midi_loop", MIDI_Loop_f);

	bFileOpen = 0;
	bPlaying = 0;
	bLooped = 1;
	bPaused = 0;
	bMidiInited = 1;

	return true;
}

static int MIDI_ExtractFile (FILE *inFile, const char *Name, size_t size)
{
	FILE		*outFile;
	size_t		remaining, count;
	int		err = 0;
	char		buf[16384];

	outFile = fopen (Name, "wb");
	if (!outFile)
		return -1;
	remaining = size;
	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, inFile);
		err = ferror(inFile);
		if (err)
			break;
		fwrite (buf, 1, count, outFile);
		err = ferror(outFile);
		if (err)
			break;
		remaining -= count;
	}

	fclose (outFile);
	return err;
}

#define	TEMP_MUSICNAME	"tmpmusic"

void MIDI_Play (const char *Name)
{
	FILE		*midiFile;
	char	midiName[MAX_OSPATH], tempName[MAX_QPATH];

	if (!bMidiInited)	//don't try to play if there is no midi
		return;

	MIDI_Stop();

	if (!Name || !*Name)
	{
		Sys_Printf("no midi music to play\n");
		return;
	}

	snprintf (tempName, sizeof(tempName), "%s.%s", Name, "mid");
	QIO_FOpenFile (va("%s/%s", "midi", tempName), &midiFile, false);
	if (!midiFile)
	{
		Con_Printf("music file %s not found\n", tempName);
		return;
	}
	else
	{
		if (file_from_pak)
		{
			int		ret;

			Con_Printf("Extracting %s from pakfile\n", tempName);
			snprintf (midiName, sizeof(midiName), "%s/%s.%s", host_parms->userdir, TEMP_MUSICNAME, "mid");
			ret = MIDI_ExtractFile (midiFile, midiName, qio_filesize);
			fclose (midiFile);
			if (ret != 0)
			{
				Con_Printf("Error while extracting from pak\n");
				return;
			}
		}
		else	/* use the file directly */
		{
			fclose (midiFile);
			snprintf (midiName, sizeof(midiName), "%s/%s/%s", qio_filepath, "midi", tempName);
		}
	}

	music = Mix_LoadMUS(midiName);
	if ( music == NULL )
	{
		Con_Printf("Couldn't load %s: %s\n", tempName, SDL_GetError());
	}
	else
	{
		bFileOpen = 1;
		Con_Printf ("Started music %s\n", tempName);
		Mix_FadeInMusic(music,0,2000);
		bPlaying = 1;
	}
}

void MIDI_Pause(int mode)
{
	if (!bPlaying)
		return;

	if ((mode == MIDI_TOGGLE_PAUSE && bPaused) || mode == MIDI_ALWAYS_RESUME)
	{
		Mix_ResumeMusic();
		bPaused = false;
	}
	else
	{
		Mix_PauseMusic();
		bPaused = true;
	}
}

void MIDI_Loop(int NewValue)
{
	if (NewValue == 2)
		bLooped = !bLooped;
	else
		bLooped = NewValue;

	MIDI_EndMusicFinished();
}

void MIDI_Stop(void)
{
	if (!bMidiInited)	//Just to be safe
		return;

	if (bFileOpen || bPlaying)
	{
		Mix_HaltMusic();
		Mix_FreeMusic(music);
	}

	bPlaying = bPaused = 0;
	bFileOpen=0;
}

void MIDI_Cleanup(void)
{
	if (bMidiInited)
	{
		MIDI_Stop();
		bMidiInited = 0;
		Con_Printf("%s: closing SDL_mixer\n", __FUNCTION__);
		Mix_CloseAudio();
	//	if (audio_wasinit == 0)
	//		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
}

