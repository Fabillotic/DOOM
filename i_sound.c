#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <AL/al.h>
#include <AL/alc.h>

#include "i_system.h"
#include "i_sound.h"
#include "w_wad.h"

#include "z_zone.h"

#include "doomdef.h"

#define SAMPLECOUNT 512

#define NUM_SOUNDS 8

ALCdevice *device;
ALCcontext *acontext;
int sounds[NUM_SOUNDS];
int sounds_start[NUM_SOUNDS];
int sources[NUM_SOUNDS];
int buffers[NUMSFX]; //Buffer 0 for the music, other buffers for sound effects

#define SONG 1
int music_source;

//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void* getsfx(char* sfxname, int* len) {
	unsigned char* sfx;
	unsigned char* paddedsfx;
	int i;
	int size;
	int paddedsize;
	char name[20];
	int sfxlump;
	
	// Get the sound data from the WAD, allocate lump
	//  in zone memory.
	sprintf(name, "ds%s", sfxname);
	
	// Now, there is a severe problem with the
	//  sound handling, in it is not (yet/anymore)
	//  gamemode aware. That means, sounds from
	//  DOOM II will be requested even with DOOM
	//  shareware.
	// The sound list is wired into sounds.c,
	//  which sets the external variable.
	// I do not do runtime patches to that
	//  variable. Instead, we will use a
	//  default sound for replacement.
	if(W_CheckNumForName(name) == -1) sfxlump = W_GetNumForName("dspistol");
	else sfxlump = W_GetNumForName(name);
	
	size = W_LumpLength( sfxlump );
	
	// Debug.
	// fprintf( stderr, "." );
	//fprintf( stderr, " -loading  %s (lump %d, %d bytes)\n",
	//	     sfxname, sfxlump, size );
	//fflush( stderr );
	
	sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );
	
	// Okay okay I think the original comments here
	// weren't made for small-brained individuals like me.
	// These two lines make sure that while the 8 header
	// bytes are preserved, the samples are padded to be
	// in multiples of SAMPLECOUNT bytes.
	paddedsize = ((size-8 + (SAMPLECOUNT-1)) / SAMPLECOUNT) * SAMPLECOUNT;
	paddedsfx = (unsigned char*)Z_Malloc( paddedsize+8, PU_STATIC, 0 );
	
	// Now copy and pad.
	memcpy(  paddedsfx, sfx, size );
	for (i=size ; i<paddedsize+8 ; i++)
        paddedsfx[i] = 128;
	
	// Remove the cached lump.
	Z_Free( sfx );
	
	// Preserve padded length.
	*len = paddedsize;
	
	// Return allocated padded data.
	return (void *) (paddedsfx + 8);
}

void I_InitSound() {
	int i;
	uint16_t freq;
	sfxinfo_t t;
	
	device = alcOpenDevice(NULL);
	printf("Device: %p\n", device);
	acontext = alcCreateContext(device, NULL);
	printf("Context: %p\n", acontext);
	alcMakeContextCurrent(acontext);
	
	alGenBuffers(NUMSFX, buffers);
	alGenSources(NUM_SOUNDS, sources);
	
	printf("Clearing sounds...\n");
	for(i = 0; i < NUM_SOUNDS; i++) {
		sounds[i] = -1;
	}
	
	printf("Iterating soundfx...\n");
	for(i = 1; i < NUMSFX; i++) {
		printf("sfx[%d]: %s\n", i, S_sfx[i].name);
		t = S_sfx[i];
		printf("Name: '%s', singu: %d, prio: %d, link: %p, pitch: %d, volume: %d, data: %p, usefulness: %d, lumpnum: %d\n", t.name, t.singularity, t.priority, t.link, t.pitch, t.volume, t.data, t.usefulness, t.lumpnum);
		
		if(!S_sfx[i].link) {
			S_sfx[i].data = getsfx(S_sfx[i].name, &S_sfx[i].length);
		}
		else {
			S_sfx[i].data = S_sfx[i].link->data;
			S_sfx[i].length = S_sfx[(S_sfx[i].link - S_sfx)/sizeof(sfxinfo_t)].length;
		}
		
		freq = *((uint16_t*) (S_sfx[i].data - 6));
		printf("Sample rate: %d\n", freq);
		alBufferData(buffers[i], AL_FORMAT_MONO8, S_sfx[i].data, S_sfx[i].length, freq);
	}
}

void I_SetChannels() {
}

void I_SetMusicVolume(int volume) {
	snd_MusicVolume = volume;
}

int I_GetSfxLumpNum(sfxinfo_t* sfx) {
	char namebuf[9];
	sprintf(namebuf, "ds%s", sfx->name);
	return W_GetNumForName(namebuf);
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority) {
	int i, o, ot;
	
	for(i = 0; i < NUM_SOUNDS; i++) {
		if(sounds[i] < 0) {
			sounds[i] = id;
			sounds_start[i] = I_GetTime();
			break;
		}
	}
	
	if(i == NUM_SOUNDS) {
		printf("Max sounds reached!\n");
		
		o = -1;
		ot = 0;
		for(i = 0; i < NUM_SOUNDS; i++) {
			if(sounds_start[i] > ot) {
				o = i;
				ot = sounds_start[i];
			}
		}
		
		alSourceStop(sources[o]);
		i = o;
	}
	
	sounds[i] = id;
	alSourcei(sources[i], AL_BUFFER, buffers[id]);
	alSourcePlay(sources[i]);
	
	return i;
}

void I_StopSound(int handle) {
	alSourceStop(sources[handle]);
	sounds[handle] = -1;
}

int I_SoundIsPlaying(int handle) {
	int state;
	
	alGetSourcei(sources[handle], AL_SOURCE_STATE, &state);
	return state == AL_PLAYING;
}

void I_UpdateSound() {
	int i;
	
	for(i = 0; i < NUM_SOUNDS; i++) {
		if(sounds[i] != -1) {
			if(!I_SoundIsPlaying(i)) {
				I_StopSound(i);
			}
		}
	}
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch) {
}

void I_ShutdownSound() {
}

void I_InitMusic() {
}

void I_ShutdownMusic() {
}

void I_PlaySong(int handle, int looping) {
	if(handle == SONG) {
		//alSourcePlay(music_source);
	}
}

void I_PauseSong(int handle) {
	if(handle == SONG) {
		//alSourcePause(music_source);
	}
}

void I_ResumeSong(int handle) {
	if(handle == SONG) {
		//alSourcePlay(music_source);
	}
}

void I_StopSong(int handle) {
	if(handle == SONG) {
		//alSourceStop(music_source);
	}
}

void I_UnRegisterSong(int handle) {
}

int I_RegisterSong(void* d) {
	return SONG;
}

int I_QrySongPlaying(int handle) {
	if(handle == SONG) {
	}
	return 0;
}
