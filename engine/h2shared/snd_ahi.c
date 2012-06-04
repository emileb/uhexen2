/*
 * Sound support using Amiga AHI, based on original work by
 * Mark Olsen <bigfoot@private.dk>
 * Adapted to uHexen2 by Szilard Biro <col.lawrence@gmail.com>
 *
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "quakedef.h"
#include "snd_sys.h"

#if HAVE_AHI_SOUND

#include "snd_ahi.h"

#include <exec/exec.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#define USE_INLINE_STDARG
#include <proto/ahi.h>
#include <utility/hooks.h>

#ifdef __AROS__
#define BUFFER_SIZE 2048
#else
#define BUFFER_SIZE 16384
#endif

static char s_ahi_driver[] = "AHI audio system";

struct Library *AHIBase;
struct AHIdata *ad;

struct AHIChannelInfo
{
	struct AHIEffChannelInfo aeci;
	ULONG offset;
};

struct AHIdata
{
	struct MsgPort *msgport;
	struct AHIRequest *ahireq;
	int ahiopen;
	struct AHIAudioCtrl *audioctrl;
	void *samplebuffer;
	struct Hook EffectHook;
	struct AHIChannelInfo aci;
	unsigned int readpos;
};

#if !defined(__AROS__)
ULONG EffectFunc()
{
	struct Hook *hook = (struct Hook *)REG_A0;
	struct AHIEffChannelInfo *aeci = (struct AHIEffChannelInfo *)REG_A1;

	struct AHIdata *ad;

	ad = hook->h_Data;
	ad->readpos = aeci->ahieci_Offset[0];

	return 0;
}

static struct EmulLibEntry EffectFunc_Gate =
{
	TRAP_LIB, 0, (void (*)(void))EffectFunc
};
#else
AROS_UFH3(ULONG, EffectFunc,
	AROS_UFHA(struct Hook *, hook, A0),
	AROS_UFHA(struct AHIAudioCtrl *, aac, A2),
	AROS_UFHA(struct AHIEffChannelInfo *, aeci, A1)
	)
{
    AROS_USERFUNC_INIT

	struct AHIdata *ad;

	ad = hook->h_Data;
	ad->readpos = aeci->ahieci_Offset[0];

	return 0;

    AROS_USERFUNC_EXIT
}
#endif

static qboolean S_AHI_Init(dma_t *dma)
{
	ULONG channels, speed, bits;
	ULONG r;
	struct AHISampleInfo sample;
	char modename[64];

	if (ad)
		return true;

	shm = dma;
	bzero(shm, sizeof(*shm));

	speed = desired_speed;

	ad = AllocVec(sizeof(*ad), MEMF_ANY);
	if (!ad)
		return false;

	ad->msgport = CreateMsgPort();
	if (ad->msgport)
	{
		ad->ahireq = (struct AHIRequest *)CreateIORequest(ad->msgport, sizeof(struct AHIRequest));
		if (ad->ahireq)
		{
			ad->ahiopen = !OpenDevice(AHINAME, AHI_NO_UNIT, (struct IORequest *)ad->ahireq, 0);
			if (ad->ahiopen)
			{
				AHIBase = (struct Library *)ad->ahireq->ahir_Std.io_Device;

				ad->audioctrl = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID,
									AHIA_MixFreq, speed,
									AHIA_Channels, 1,
									AHIA_Sounds, 1,
									TAG_END);

				if (ad->audioctrl)
				{
					AHI_GetAudioAttrs(AHI_INVALID_ID, ad->audioctrl,
								AHIDB_BufferLen, sizeof(modename),
								AHIDB_Name, (ULONG)modename,
								AHIDB_MaxChannels, (ULONG)&channels,
								AHIDB_Bits, (ULONG)&bits,
								TAG_END);

					AHI_ControlAudio(ad->audioctrl,
								AHIC_MixFreq_Query, (ULONG)&speed,
								TAG_END);

					if (bits == 8 || bits == 16)
					{
						if (channels > 2)
							channels = 2;

						shm->speed = speed;
						shm->samplebits = bits;
						shm->channels = channels;
						shm->samples = BUFFER_SIZE*(speed/11025);
						shm->submission_chunk = 1;

						ad->samplebuffer = AllocVec(BUFFER_SIZE*(speed/11025)*(bits/8)*channels, MEMF_ANY);
						if (ad->samplebuffer)
						{
							shm->buffer = ad->samplebuffer;

							if (channels == 1)
							{
								if (bits == 8)
									sample.ahisi_Type = AHIST_M8S;
								else	sample.ahisi_Type = AHIST_M16S;
							}
							else
							{
								if (bits == 8)
									sample.ahisi_Type = AHIST_S8S;
								else	sample.ahisi_Type = AHIST_S16S;
							}

							sample.ahisi_Address = ad->samplebuffer;
							sample.ahisi_Length = (BUFFER_SIZE*(speed/11025)*(bits/8))/AHI_SampleFrameSize(sample.ahisi_Type);

							r = AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, ad->audioctrl);
							if (r == 0)
							{
								r = AHI_ControlAudio(ad->audioctrl,
											AHIC_Play, TRUE,
											TAG_END);

								if (r == 0)
								{
									AHI_Play(ad->audioctrl,
										AHIP_BeginChannel, 0,
										AHIP_Freq, speed,
										AHIP_Vol, 0x10000,
										AHIP_Pan, 0x8000,
										AHIP_Sound, 0,
										AHIP_EndChannel, NULL,
										TAG_END);

									ad->aci.aeci.ahie_Effect = AHIET_CHANNELINFO;
									ad->aci.aeci.ahieci_Func = &ad->EffectHook;
									ad->aci.aeci.ahieci_Channels = 1;

#if !defined(__AROS__)
									ad->EffectHook.h_Entry = (void *)&EffectFunc_Gate;
#else
									ad->EffectHook.h_Entry = (IPTR (*)())&EffectFunc;
#endif
									ad->EffectHook.h_Data = ad;
									AHI_SetEffect(&ad->aci, ad->audioctrl);

									Con_Printf("Using AHI mode \"%s\" for audio output\n", modename);
									Con_Printf("Channels: %d bits: %d frequency: %d\n", channels, bits, speed);

									return true;
								}
							}
						}
						FreeVec(ad->samplebuffer);
					}
					AHI_FreeAudio(ad->audioctrl);
				}
				else
					Con_Printf("Failed to allocate AHI audio\n");

				CloseDevice((struct IORequest *)ad->ahireq);
			}
			DeleteIORequest((struct IORequest *)ad->ahireq);
		}
		DeleteMsgPort(ad->msgport);
	}
	FreeVec(ad);
	ad = NULL;
	AHIBase = NULL;

	return false;
}

static int S_AHI_GetDMAPos(void)
{
	return ad->readpos*shm->channels;
}

static void S_AHI_Shutdown(void)
{
	if (ad == NULL)
		return;

	ad->aci.aeci.ahie_Effect = AHIET_CHANNELINFO|AHIET_CANCEL;
	AHI_SetEffect(&ad->aci.aeci, ad->audioctrl);
	AHI_ControlAudio(ad->audioctrl, AHIC_Play, FALSE, TAG_END);

	AHI_FreeAudio(ad->audioctrl);
	FreeVec(ad->samplebuffer);
	CloseDevice((struct IORequest *)ad->ahireq);
	DeleteIORequest((struct IORequest *)ad->ahireq);
	DeleteMsgPort(ad->msgport);
	FreeVec(ad);

	ad = NULL;
	AHIBase = NULL;
}

static void S_AHI_LockBuffer (void)
{
}

static void S_AHI_BlockSound (void)
{
}

static void S_AHI_UnblockSound (void)
{
}

static void S_AHI_Submit(void)
{
}

snd_driver_t snddrv_ahi =
{
	S_AHI_Init,
	S_AHI_Shutdown,
	S_AHI_GetDMAPos,
	S_AHI_LockBuffer,
	S_AHI_Submit,
	S_AHI_BlockSound,
	S_AHI_UnblockSound,
	s_ahi_driver,
	SNDDRV_ID_AHI,
	false,
	NULL
};

#endif	/* HAVE_AHI_SOUND */

