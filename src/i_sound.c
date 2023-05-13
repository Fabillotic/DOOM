//-----------------------------------------------------------------------------
//
// Copyright (C) 2022 by Fabillotic
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// DESCRIPTION:
//	System interface for sound.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <AL/al.h>
#include <AL/alc.h>

#ifdef FLUIDSYNTH
#include <fluidsynth.h>
#endif

#include "i_sound.h"
#include "i_system.h"
#include "m_argv.h"
#include "s_sound.h"
#include "w_wad.h"

#include "doomstat.h"
#include "p_mobj.h"

#include "tables.h"

#include "z_zone.h"

#include "doomdef.h"

#define SAMPLERATE 44100
/* SAMPLERATE / 140Hz */
#define SAMPLES_PER_TICK 315
#define SAMPLECOUNT 512
#define NUM_SOUNDS 32
#define MUSIC_TICKS_PER_ITERATION 20

#define PI 3.141592653589

char *soundfont = NULL;

ALCdevice *device;
ALCcontext *acontext;

#ifdef FLUIDSYNTH
fluid_synth_t *fsynth;
fluid_sequencer_t *fsequencer;
#endif

int sounds[NUM_SOUNDS];
int sounds_start[NUM_SOUNDS];
int sources[NUM_SOUNDS];
int buffers[NUMSFX - 1];
float orientation[6];

#define SONG 1
int music_source;
music_buffer_t *music_buffers;
int music_ticks;
int song_play;

pthread_t music_thread;
volatile sig_atomic_t music_stop;

void *getsfx(char *sfxname, int *len);
mus_event_t *parse_data(char *data);
unsigned char get_midi_channel(unsigned char channel);

#ifdef FLUIDSYNTH
int sequence(mus_event_t *events, fluid_synth_t *synth, fluid_sequencer_t *sequencer);
void* synthesize(void* arg);
#endif

void I_InitSound() {
	int i;
	uint16_t freq;
	sfxinfo_t t;

	device = alcOpenDevice(NULL);
	printf("Device: %p\n", device);
	acontext = alcCreateContext(device, NULL);
	printf("Context: %p\n", acontext);
	alcMakeContextCurrent(acontext);

	alGenBuffers(NUMSFX - 1, buffers);
	alGenSources(NUM_SOUNDS, sources);

	for(i = 0; i < NUM_SOUNDS; i++) {
		sounds[i] = -1;
		alSourcei(sources[i], AL_REFERENCE_DISTANCE, S_CLOSE_DIST);
		alSourcei(sources[i], AL_MAX_DISTANCE, S_CLIPPING_DIST);
	}

	printf("Iterating soundfx...\n");
	for(i = 1; i < NUMSFX; i++) {
		printf("sfx[%d]: %s\n", i, S_sfx[i].name);
		t = S_sfx[i];

		if(!S_sfx[i].link) {
			S_sfx[i].data = getsfx(S_sfx[i].name, &S_sfx[i].length);
		}
		else {
			S_sfx[i].data = S_sfx[i].link->data;
			S_sfx[i].length =
			    S_sfx[(S_sfx[i].link - S_sfx) / sizeof(sfxinfo_t)].length;
		}

		freq = *((uint16_t *) (S_sfx[i].data - 6));
		alBufferData(buffers[i - 1], AL_FORMAT_MONO8, S_sfx[i].data,
		    S_sfx[i].length, freq);
	}

	I_InitMusic();
}

void I_InitMusic() {
#ifdef FLUIDSYNTH
	int p;
	fluid_settings_t *settings;
#endif

#ifdef FLUIDSYNTH
	soundfont = getenv("SOUNDFONT");
	if(!soundfont) {
		if((p = M_CheckParm("-soundfont"))) {
			if(p < myargc - 1) {
				soundfont = myargv[p + 1];
			}
		}
	}
	if(!soundfont) {
		printf("No soundfont found!\n");
		I_Quit();
	}
	if(access(soundfont, F_OK)) {
		printf("Could not open soundfont!\n");
		I_Quit();
	}

	settings = new_fluid_settings();
	fluid_settings_setnum(settings, "synth.sample-rate", (float) SAMPLERATE);

	fsynth = new_fluid_synth(settings);

	fluid_synth_sfload(fsynth, soundfont, 1);
#endif

	alGenSources(1, &music_source);

	music_thread = NULL;
}

void I_SetChannels() {
}

void I_SetMusicVolume(int volume) {
	snd_MusicVolume = volume;
}

int I_GetSfxLumpNum(sfxinfo_t *sfx) {
	char namebuf[9];
	sprintf(namebuf, "ds%s", sfx->name);
	return W_GetNumForName(namebuf);
}

int I_StartSound(
    int id, int vol, int sep, int pitch, int priority, mobj_t *origin) {
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
		ot = INT_MAX;
		for(i = 0; i < NUM_SOUNDS; i++) {
			if(sounds_start[i] < ot) {
				o = i;
				ot = sounds_start[i];
			}
		}

		alSourceStop(sources[o]);
		i = o;
	}

	sounds[i] = id;
	alSourcei(sources[i], AL_BUFFER, buffers[id - 1]);
	alSourcef(sources[i], AL_GAIN, vol / 15.0f);

	if(origin && origin != players[consoleplayer].mo) {
		alSourcei(sources[i], AL_SOURCE_RELATIVE, AL_FALSE);
		alSource3i(sources[i], AL_POSITION, origin->x, 0, origin->y);
	}
	else {
		/* Make the source "sticky" */
		alSourcei(sources[i], AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3i(sources[i], AL_POSITION, 0, 0, 0);
	}

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
	double angle;

	if(players[consoleplayer].mo) {
		alListener3i(AL_POSITION, players[consoleplayer].mo->x, 0,
		    players[consoleplayer].mo->y);
		angle = (players[consoleplayer].mo->angle >> ANGLETOFINESHIFT) /
		        1024.0 * (PI / 4.0);

		orientation[0] = (float) cos(angle + PI);
		orientation[1] = 0.0f;
		orientation[2] = (float) sin(angle + PI);

		orientation[3] = 0.0f;
		orientation[4] = 1.0f;
		orientation[5] = 0.0f;
		alListenerfv(AL_ORIENTATION, orientation);
	}
	else {
		alListener3i(AL_POSITION, 0, 0, 0);
	}

	for(i = 0; i < NUM_SOUNDS; i++) {
		if(sounds[i] != -1) {
			if(!I_SoundIsPlaying(i)) {
				I_StopSound(i);
			}
		}
	}

	alSourcef(music_source, AL_GAIN, snd_MusicVolume / 15.0f);
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch) {
}

void I_ShutdownSound() {
}

void I_ShutdownMusic() {
}

void I_PlaySong(int handle, int looping) {
	printf("PlaySong: %d, looping: %d\n", handle, looping);
	if(handle == SONG) {
		alSourcei(music_source, AL_LOOPING, looping);
		alSourcePlay(music_source);
		song_play = 1;
	}
}

void I_PauseSong(int handle) {
	if(handle == SONG) {
		alSourcePause(music_source);
		song_play = 0;
	}
}

void I_ResumeSong(int handle) {
	if(handle == SONG) {
		alSourcePlay(music_source);
		song_play = 1;
	}
}

void I_StopSong(int handle) {
	printf("StopSong: %d\n", handle);
	if(handle == SONG) {
		alSourceStop(music_source);
		song_play = 0;
	}
}

void I_UnRegisterSong(int handle) {
}

int I_RegisterSong(void *data) {
	mus_event_t *events;
	music_buffer_t *buf, *buf2;

	printf("I_RegisterSong!\n");

	if(music_thread) {
		music_stop = 1;
		printf("Waiting for music thread to stop...\n");
		pthread_join(music_thread, NULL);
		printf("Music thread stopped!\n");
		music_thread = NULL;
	}

#ifdef FLUIDSYNTH
	if(fsequencer) {
		delete_fluid_sequencer(fsequencer);
	}
#endif

	alSourcei(music_source, AL_BUFFER, 0);
	for(buf = music_buffers; buf; buf = buf2) {
		buf2 = buf->next;
		alDeleteBuffers(1, &(buf->name));
		free(buf);
	}
	music_buffers = NULL;

	events = parse_data(data);

#ifdef FLUIDSYNTH
	fsequencer = new_fluid_sequencer2(0);
	fluid_synth_system_reset(fsynth);

	music_ticks = sequence(events, fsynth, fsequencer);
#endif

	song_play = 0;
	music_stop = 0;

#ifdef FLUIDSYNTH
	pthread_create(&music_thread, NULL, synthesize, NULL);
#endif

	return SONG;
}

#ifdef FLUIDSYNTH
void* synthesize(void* arg) {
	int i, buffer_name, wsize;
	void *wdata;
	music_buffer_t *new_buffer, *buf;

	for(i = 0; i < music_ticks / MUSIC_TICKS_PER_ITERATION + 1; i++) {
		if(music_stop) {
			return NULL;
		}

		//printf("Synthesizing chunk %d / %d...\n", i, music_ticks / MUSIC_TICKS_PER_ITERATION + 1);

		wsize = 2 * 2 * SAMPLES_PER_TICK * MUSIC_TICKS_PER_ITERATION;
		wdata = malloc(wsize);

		fluid_synth_write_s16(fsynth, SAMPLES_PER_TICK * MUSIC_TICKS_PER_ITERATION, wdata, 0, 2, wdata, 1, 2);

		alGenBuffers(1, &buffer_name);
		alBufferData(buffer_name, AL_FORMAT_STEREO16, wdata, wsize, SAMPLERATE);

		alSourceQueueBuffers(music_source, 1, &buffer_name);

		new_buffer = malloc(sizeof(music_buffer_t));
		new_buffer->next = NULL;
		new_buffer->name = buffer_name;

		for(buf = music_buffers; buf && buf->next; buf = buf->next);
		if(buf) buf->next = new_buffer;
		else music_buffers = new_buffer;

		free(wdata);

		if(i == 0) {
			//If the main thread already attempted to play the song
			if(song_play) alSourcePlay(music_source);
		}
	}
	printf("Done synthesizing!\n");

	return NULL;
}
#endif

int I_QrySongPlaying(int handle) {
	printf("I_QrySongPlaying!\n");
	if(handle == SONG) {}
	return 0;
}

//
// Copyright (C) 1993-1996 by id Software, Inc.
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void *getsfx(char *sfxname, int *len) {
	unsigned char *sfx;
	unsigned char *paddedsfx;
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

	size = W_LumpLength(sfxlump);

	// Debug.
	// fprintf( stderr, "." );
	// fprintf( stderr, " -loading  %s (lump %d, %d bytes)\n",
	//	     sfxname, sfxlump, size );
	// fflush( stderr );

	sfx = (unsigned char *) W_CacheLumpNum(sfxlump, PU_STATIC);

	// Okay okay I think the original comments here
	// weren't made for small-brained individuals like me.
	// These two lines make sure that while the 8 header
	// bytes are preserved, the samples are padded to be
	// in multiples of SAMPLECOUNT bytes.
	paddedsize = ((size - 8 + (SAMPLECOUNT - 1)) / SAMPLECOUNT) * SAMPLECOUNT;
	paddedsfx = (unsigned char *) Z_Malloc(paddedsize + 8, PU_STATIC, 0);

	// Now copy and pad.
	memcpy(paddedsfx, sfx, size);
	for(i = size; i < paddedsize + 8; i++) paddedsfx[i] = 128;

	// Remove the cached lump.
	Z_Free(sfx);

	// Preserve padded length.
	*len = paddedsize;

	// Return allocated padded data.
	return (void *) (paddedsfx + 8);
}

mus_event_t *parse_data(char *data) {
	int i, o, delay;
	uint16_t len, offcheck, p_ch, s_ch, n_instr;
	unsigned char type, channel, ev, de;
	char magic[4];
	mus_event_t *events, *event, *tmp;

	magic[0] = 'M';
	magic[1] = 'U';
	magic[2] = 'S';
	magic[3] = 0x1a;

	if(memcmp(data, magic, 4 * sizeof(char))) {
		printf("Invalid MUS file! Missing magic header.\n");
		return NULL;
	}

	o = 4;
	len = *((uint16_t *) (data + o));
	o += 2;
	offcheck = *((uint16_t *) (data + o));
	o += 2;
	p_ch = *((uint16_t *) (data + o));
	o += 2;
	s_ch = *((uint16_t *) (data + o));
	o += 2;
	n_instr = *((uint16_t *) (data + o));
	o += 2;

	/* reserved bytes */
	o += 2;

	printf("Length: %d, primary channels: %d, secondary channels: %d, "
	       "instruments: %d\n",
	    len, p_ch, s_ch, n_instr);

	for(i = 0; i < n_instr; i++) {
		printf("Instrument %d: %d\n", i, *((uint16_t *) (data + o)));
		o += 2;
	}

	if(o != offcheck) {
		printf("Song offset mismatch! Read: %d, check: %d\n", o, offcheck);
		return NULL;
	}

	events = NULL;
	while(o < len + offcheck) {
		ev = (unsigned char) data[o];
		o++;

		event = malloc(sizeof(mus_event_t));

		type = (ev >> 4) & 7;
		channel = ev & 15;

		event->type = type;
		event->channel = channel;

		if(type == MUS_RELEASE) {
			event->dlength = 1;
			event->data[0] = (unsigned char) data[o] & 127;
			o++;
		}
		else if(type == MUS_PRESS) {
			event->dlength = 1;
			event->data[0] = (unsigned char) data[o];
			o++;
			if((event->data[0] >> 7) > 0) {
				event->dlength++;
				event->data[0] &= 127;
				event->data[1] = (unsigned char) data[o] & 127;
				o++;
			}
		}
		else if(type == MUS_PITCH) {
			event->dlength = 1;
			event->data[0] = (unsigned char) data[o];
			o++;
		}
		else if(type == MUS_SYSTEM) {
			event->dlength = 1;
			event->data[0] = (unsigned char) data[o] & 127;
			o++;
		}
		else if(type == MUS_CONTROL) {
			event->dlength = 2;
			event->data[0] = (unsigned char) data[o] & 127;
			o++;
			event->data[1] = (unsigned char) data[o] & 127;
			o++;
		}
		else if(type == MUS_MEASURE) {}
		else if(type == MUS_FINISH) {}
		else if(type == MUS_UNUSED) {
			/* Skip unused data */
			o++;
		}

		delay = 0;
		de = ev;
		while((de >> 7) > 0) {
			de = (unsigned char) data[o];
			delay <<= 7;
			delay += de & 127;
			o++;
		}

		event->delay = delay;
		event->next = NULL;

		for(tmp = events; tmp; tmp = tmp->next) {
			if(!tmp->next) {
				tmp->next = event;
				break;
			}
		}
		if(!events) events = event;

		// printf("Event! Type: %d, channel: %d, dlength: %d, delay: %d\n",
		// event->type, event->channel, event->dlength, event->delay);
	}

	return events;
}

#ifdef FLUIDSYNTH
int sequence(mus_event_t *events, fluid_synth_t *synth, fluid_sequencer_t *sequencer) {
	int i, ticks;
	mus_event_t *event;
	unsigned char volume[16];
	unsigned char midi_channel;
	uint16_t pitch;
	short synth_dest;
	fluid_event_t *fluid_ev;
	fluid_midi_event_t *fluid_midi_ev;

	synth_dest = fluid_sequencer_register_fluidsynth(sequencer, synth);
	fluid_sequencer_set_time_scale(sequencer, 140);

	ticks = fluid_sequencer_get_tick(sequencer);

	for(i = 0; i < 16; i++) volume[i] = 100;

	for(event = events; event; event = event->next) {
		midi_channel = get_midi_channel(event->channel);

		if(event->type == MUS_PRESS) {
			// printf("MUS_PRESS: channel(%d), note(%d)\n", event->channel,
			// event->data[0]);
			if(event->dlength > 1) volume[event->channel] = event->data[1];

			fluid_ev = new_fluid_event();
			fluid_event_set_source(fluid_ev, -1);
			fluid_event_set_dest(fluid_ev, synth_dest);
			fluid_event_noteon(
			    fluid_ev, midi_channel, event->data[0], volume[event->channel]);
			fluid_sequencer_send_at(sequencer, fluid_ev, ticks, 1);
			delete_fluid_event(fluid_ev);
		}
		else if(event->type == MUS_RELEASE) {
			// printf("MUS_RELEASE: channel(%d), note(%d)\n", event->channel,
			// event->data[0]);

			fluid_ev = new_fluid_event();
			fluid_event_set_source(fluid_ev, -1);
			fluid_event_set_dest(fluid_ev, synth_dest);
			fluid_event_noteoff(fluid_ev, midi_channel, event->data[0]);
			fluid_sequencer_send_at(sequencer, fluid_ev, ticks, 1);
			delete_fluid_event(fluid_ev);
		}
		else if(event->type == MUS_PITCH) {
			// printf("MUS_PITCH: channel(%d), bend(%d)\n", event->channel,
			// event->data[0]);

			fluid_ev = new_fluid_event();
			fluid_event_set_source(fluid_ev, -1);
			fluid_event_set_dest(fluid_ev, synth_dest);
			pitch = ((uint16_t) event->data[0]) * 64;
			fluid_event_pitch_bend(fluid_ev, midi_channel, pitch);
			fluid_sequencer_send_at(sequencer, fluid_ev, ticks, 1);
			delete_fluid_event(fluid_ev);
		}
		else if(event->type == MUS_CONTROL) {
			// printf("MUS_CONTROL: channel(%d), ctrl(%d), value(%d)\n",
			// event->channel, event->data[0], event->data[1]);
			if(event->data[0] == 0) {
				fluid_ev = new_fluid_event();
				fluid_event_set_source(fluid_ev, -1);
				fluid_event_set_dest(fluid_ev, synth_dest);
				fluid_event_program_change(
				    fluid_ev, midi_channel, event->data[1]);
				fluid_sequencer_send_at(sequencer, fluid_ev, ticks, 1);
				delete_fluid_event(fluid_ev);
			}
			else if(event->data[0] == 1) {
				fluid_ev = new_fluid_event();
				fluid_event_set_source(fluid_ev, -1);
				fluid_event_set_dest(fluid_ev, synth_dest);
				fluid_event_bank_select(fluid_ev, midi_channel, event->data[1]);
				fluid_sequencer_send_at(sequencer, fluid_ev, ticks, 1);
				delete_fluid_event(fluid_ev);
			}
			else if(event->data[0] == 4) {
				fluid_ev = new_fluid_event();
				fluid_event_set_source(fluid_ev, -1);
				fluid_event_set_dest(fluid_ev, synth_dest);
				fluid_event_control_change(
				    fluid_ev, midi_channel, 10, event->data[1]);
				fluid_sequencer_send_at(sequencer, fluid_ev, ticks, 1);
				delete_fluid_event(fluid_ev);
			}
		}
		ticks += event->delay;
	}
	printf("Scheduling done.\n");

	return ticks;
}
#endif

unsigned char get_midi_channel(unsigned char channel) {
	if(channel == 15) return 9;
	return channel;
}
