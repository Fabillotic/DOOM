# DOOM

Modern linux version of DOOM

## THIS BRANCH IS ABANDONED!! Just use XWayland, it works better anyways

## Major changes

- X11 code rewrite
- OpenAL sounds
- Fluidsynth MIDI music

## Dependencies

- X11
- OpenAL
- Fluidsynth
- duh...

## Additional requirements

### Appropriate soundfont

Recommended: [https://archive.org/download/SC55EmperorGrieferus/Roland%20SC-55%20v3.7.sf2](https://archive.org/download/SC55EmperorGrieferus/Roland%20SC-55%20v3.7.sf2)

### WADfile

You're going to need a WADfile for playing DOOM.
I recommend you buy a copy of the original game through Steam.

(By the way, DOOM Eternal also gives you the WADs for DOOM I and II)

## Instructions

1. Clone the code
2. Edit the `SOUNDFONT=` line in the `Makefile` to point to the soundfont you wish to use
3. Compile with `make`
4. Run `make run`, it's going to tell you that no WADfiles were found
5. Put the WADfile in the `wads` folder
6. Run it again
7. Have fun!

## Movement

### Default

- You can use WASD to move around and use your mouse to look left/right.
- The interact button is 'e'.
- Sprinting is on 'LSHIFT'.
- You can shoot with the left mouse button.

### Original movement

You can go back to the original movement by disabling the `-DFPSMOVE` flag in the `Makefile` and running `make -B` to recompile.

## License

This project is licensed under the GPLv2 license.
See `LICENSE` for a copy of the text.
