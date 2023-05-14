# DOOM

Modern linux version of DOOM

## Major changes

- X11 code rewrite
- OpenAL sounds
- Fluidsynth / ALSA MIDI music

## Dependencies

- X11
- OpenAL
- FluidSynth or ALSA for music
- OpenGL + GLEW (not strictly necessary, see below)

## Additional requirements

### FluidSynth: Appropriate soundfont

If you want to use FluidSynth, you will also need a soundfont.

Recommended: [https://archive.org/download/SC55EmperorGrieferus/Roland%20SC-55%20v3.7.sf2](https://archive.org/download/SC55EmperorGrieferus/Roland%20SC-55%20v3.7.sf2)

### WADfile

You're going to need a WADfile for playing DOOM.
I recommend you buy a copy of the original game through Steam.

(By the way, DOOM Eternal also gives you the WADs for DOOM I and II)

## Instructions

### Part 1: Basics

1. Clone the code

### Part 2: Setting up the music

#### - FluidSynth (recommended):

1. Edit the `SOUNDFONT=` line in the `Makefile` to point to the soundfont you wish to use
2. Make sure the line `USE_FLUIDSYNTH=1` is not commented out (so make sure that it does not have a hashtag in front of it)
3. Make sure that the `MUSIC_TYPE` is set to `fluidsynth`.

#### - ALSA_SEQ:

1. Edit the `MIDI_PORT=` line in the `Makefile` to the MIDI port that your MIDI synthesizer uses (You can find the connected MIDI device ports by running `aplaymidi -l`)
2. Make sure the line `USE_ALSA_SEQ=1` is not commented out (so make sure that it does not have a hashtag in front of it)
3. Make sure that the `MUSIC_TYPE` is set to `alsa_seq`.

### Part 3: Compiling and running

1. Compile with `make` (you might need to run `make -B` if you've changed the `Makefile`)
2. Put the WADfile in the `wads` folder
3. Run `make run`
4. Have fun!

## Movement

### Default

- You can use WASD to move around and use your mouse to look left/right.
- The interact key is `E`.
- Sprinting is on `LSHIFT`.
- You can shoot with the left mouse button.

### Original movement

You can go back to the original movement by disabling the `-DFPSMOVE` flag in the `Makefile` and running `make -B` to recompile.

## OpenGL

This project uses OpenGL to accelerate image processing and to display the image. (no actual graphics rewrite)

An older version of OpenGL using immediate mode can be enabled by uncommenting the `GL2=1` line in the `Makefile`.

Alternatively X11 primitives can be used for rendering by commenting out the line `USE_OPENGL=1`.

Again, you'll need to run `make -B` to recompile.

## License

This project is licensed under the GPLv2 license.
See `LICENSE` for a copy of the text.
