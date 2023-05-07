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
//	OpenGL code for compiling and linking shaders.
//
//-----------------------------------------------------------------------------

#ifndef __SHADERCODE__
#define __SHADERCODE__

int load_shader(char *vertexShaderPath, char *geometryShaderPath, char *fragmentShaderPath);
int create_shader(char *vertexShaderSource, char *geometryShaderSource, char *fragmentShaderSource);

#endif
