#include <stdio.h>
#include <stdlib.h>

#include "i_system.h"
#include "i_sound.h"
#include "w_wad.h"

#include "z_zone.h"

#include "doomdef.h"

#define SAMPLECOUNT 512

int lengths[NUMSFX];

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
	sfxinfo_t t;
	
	for(i = 1; i < NUMSFX; i++) {
		t = S_sfx[i];
		printf("Name: '%s', singu: %d, prio: %d, link: %d, pitch: %d, volume: %d, data: %d, usefulness: %d, lumpnum: %d\n", t.name, t.singularity, t.priority, t.link, t.pitch, t.volume, t.data, t.usefulness, t.lumpnum);
		
		if(!S_sfx[i].link) {
			S_sfx[i].data = getsfx(S_sfx[i].name, lengths + i);
		}
		else {
			S_sfx[i].data = S_sfx[i].link->data;
			lengths[i] = lengths[(S_sfx[i].link - S_sfx)/sizeof(sfxinfo_t)];
		}
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
	return id;
}

void I_StopSound(int handle) {
}

int I_SoundIsPlaying(int handle) {
	return 0;
}

void I_UpdateSound() {
}

void I_SubmitSound() {
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
}

void I_PauseSong(int handle) {
}

void I_ResumeSong(int handle) {
}

void I_StopSong(int handle) {
}

void I_UnRegisterSong(int handle) {
}

int I_RegisterSong(void* data) {
	return 1;
}

int I_QrySongPlaying(int handle) {
	return 0;
}
