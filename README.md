# DOOM

Modern linux version of DOOM

## Major changes

- X11 code rewrite
- OpenAL sounds
- Fluidsynth MIDI music

## Dependencies

- X11
- OpenAL
- Fluidsynth
- OpenGL + GLEW (can be disabled, see below)

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
4. Put the WADfile in the `wads` folder
5. Run `make run`
6. Have fun!

## Movement

### Default

- You can use WASD to move around and use your mouse to look left/right.
- The interact key is `E`.
- Sprinting is on `LSHIFT`.
- You can shoot with the left mouse button.

### Original movement

You can go back to the original movement by disabling the `-DFPSMOVE` flag in the `Makefile` and running `make -B` to recompile.

## OpenGL

This project uses OpenGL to accelerate image processing and to display the image.

Alternatively X11 primitives can be used for rendering by commenting out the line "USE_OPENGL=1" in the `Makefile`.

## License

This project is licensed under the GPLv2 license.
See `LICENSE` for a copy of the text.
