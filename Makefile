##### Basic variables #####

CC=gcc # gcc or g++

CFLAGS=-g -Wall -std=c99 \
        -D_DEFAULT_SOURCE \
	-DNORMALUNIX -DLINUX \
	-DFPSMOVE \
	-DIGNORE_DEMO_VERSION \
	-DJOYSTICK
	# -DMOUSEMOVE
	# -DJOYTEST
LDFLAGS=-L/usr/X11R6/lib
LIBS=-lX11 -lm -lopenal -lpthread

# subdirectory for objects
O=linux

# subdirectory for sources
SRC=src

BIN=linuxxdoom


##### Run task options #####

# subdirectory for wadfiles
WADS=wads

# what music player to use when running (fluidsynth / alsa_seq)
MUSIC_TYPE=fluidsynth

# soundfont for FluidSynth music
SOUNDFONT="/usr/share/soundfonts/Roland SC-55 v3.7.sf2"

# set your MIDI output port here
MIDI_PORT=128:0

#set your Joystick path here
JOYSTICK=/dev/input/js1

##### Compilation flags #####

# comment out to disable OpenGL
USE_OPENGL=1

# uncomment to switch to OpenGL2 instead of the default version 3.3
# GL2=1

# comment out to compile without FluidSynth
USE_FLUIDSYNTH=1

# comment out to compile without ALSA midi playback
USE_ALSA_SEQ=1


##### Objects #####

OBJS=					\
		$(O)/doomdef.o		\
		$(O)/doomstat.o		\
		$(O)/dstrings.o		\
		$(O)/i_system.o		\
		$(O)/i_sound.o		\
		$(O)/i_video.o		\
		$(O)/i_net.o		\
		$(O)/tables.o		\
		$(O)/f_finale.o		\
		$(O)/f_wipe.o 		\
		$(O)/d_main.o		\
		$(O)/d_net.o		\
		$(O)/d_items.o		\
		$(O)/g_game.o		\
		$(O)/m_menu.o		\
		$(O)/m_misc.o		\
		$(O)/m_argv.o  		\
		$(O)/m_bbox.o		\
		$(O)/m_fixed.o		\
		$(O)/m_swap.o		\
		$(O)/m_cheat.o		\
		$(O)/m_random.o		\
		$(O)/am_map.o		\
		$(O)/p_ceilng.o		\
		$(O)/p_doors.o		\
		$(O)/p_enemy.o		\
		$(O)/p_floor.o		\
		$(O)/p_inter.o		\
		$(O)/p_lights.o		\
		$(O)/p_map.o		\
		$(O)/p_maputl.o		\
		$(O)/p_plats.o		\
		$(O)/p_pspr.o		\
		$(O)/p_setup.o		\
		$(O)/p_sight.o		\
		$(O)/p_spec.o		\
		$(O)/p_switch.o		\
		$(O)/p_mobj.o		\
		$(O)/p_telept.o		\
		$(O)/p_tick.o		\
		$(O)/p_saveg.o		\
		$(O)/p_user.o		\
		$(O)/r_bsp.o		\
		$(O)/r_data.o		\
		$(O)/r_draw.o		\
		$(O)/r_main.o		\
		$(O)/r_plane.o		\
		$(O)/r_segs.o		\
		$(O)/r_sky.o		\
		$(O)/r_things.o		\
		$(O)/w_wad.o		\
		$(O)/wi_stuff.o		\
		$(O)/v_video.o		\
		$(O)/st_lib.o		\
		$(O)/st_stuff.o		\
		$(O)/hu_stuff.o		\
		$(O)/hu_lib.o		\
		$(O)/s_sound.o		\
		$(O)/z_zone.o		\
		$(O)/info.o		\
		$(O)/sounds.o		\
		$(O)/i_main.o


##### Library logic #####

ifdef USE_OPENGL
	CFLAGS += -DOPENGL
	LIBS += -lGL -lGLEW
	OBJS += $(O)/shader.o
ifdef GL2
	CFLAGS += -DGL2
endif
endif

ifdef USE_FLUIDSYNTH
	CFLAGS += -DFLUIDSYNTH
	LIBS += -lfluidsynth
endif

ifdef USE_ALSA_SEQ
	CFLAGS += -DALSA_SEQ
	LIBS += -lasound
endif


##### Tasks #####

all:	 $(O)/$(BIN)

clean:
	rm -f $(O)/$(BIN) $(OBJS)

style:
	clang-format -style=file -i $(SRC)/*.c $(SRC)/*.h

$(O)/$(BIN):	$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) \
	-o $(O)/$(BIN) $(LIBS)

$(O)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(O)/$(BIN)
	SOUNDFONT=$(SOUNDFONT) DOOMWADDIR=$(WADS) ./$(O)/$(BIN) -3 \
		-midi-port $(MIDI_PORT) -music $(MUSIC_TYPE) -joystick $(JOYSTICK)

debug: $(O)/$(BIN)
	SOUNDFONT=$(SOUNDFONT) DOOMWADDIR=$(WADS) gdb ./$(O)/$(BIN)

.PHONY: all clean run style
