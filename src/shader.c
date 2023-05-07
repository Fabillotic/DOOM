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

#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include "shader.h"

int load_shader(char *vertexShaderPath, char *geometryShaderPath, char *fragmentShaderPath) {
	FILE *f;
	int i, l, shader;
	char *vertexShaderSource;
	char *geometryShaderSource;
	char *fragmentShaderSource;

	if(vertexShaderPath) {
		f = fopen(vertexShaderPath, "r");
		if(!f) {
			printf("Could not open vertex shader file!\n");
			return 0;
		}
		for(l = 0; fgetc(f) != EOF; l++);
		rewind(f);
		vertexShaderSource = malloc(sizeof(char) * (l + 1));
		for(i = 0; i < l; i++) vertexShaderSource[i] = fgetc(f);
		vertexShaderSource[i] = '\0';
		fclose(f);
	}
	else vertexShaderSource = NULL;

	if(fragmentShaderPath) {
		f = fopen(fragmentShaderPath, "r");
		if(!f) {
			printf("Could not open fragment shader file!\n");
			return 0;
		}
		for(l = 0; fgetc(f) != EOF; l++);
		rewind(f);
		fragmentShaderSource = malloc(sizeof(char) * (l + 1));
		for(i = 0; i < l; i++) fragmentShaderSource[i] = fgetc(f);
		fragmentShaderSource[i] = '\0';
		fclose(f);
	}
	else fragmentShaderSource = NULL;

	if(geometryShaderPath) {
		f = fopen(geometryShaderPath, "r");
		if(!f) {
			printf("Could not open geometry shader file!\n");
			return 0;
		}
		for(l = 0; fgetc(f) != EOF; l++);
		rewind(f);
		geometryShaderSource = malloc(sizeof(char) * (l + 1));
		for(i = 0; i < l; i++) geometryShaderSource[i] = fgetc(f);
		geometryShaderSource[i] = '\0';
		fclose(f);
	}
	else geometryShaderSource = NULL;

	if(vertexShaderSource) printf("Vertex shader:\n%s", vertexShaderSource);
	if(geometryShaderSource) printf("Geometry shader:\n%s", geometryShaderSource);
	if(fragmentShaderSource) printf("Fragment shader:\n%s", fragmentShaderSource);

	shader = create_shader(vertexShaderSource, geometryShaderSource, fragmentShaderSource);
	if(vertexShaderSource) free(vertexShaderSource);
	if(fragmentShaderSource) free(fragmentShaderSource);
	if(geometryShaderSource) free(geometryShaderSource);

	return shader;
}

int create_shader(char *vertexShaderSource, char *geometryShaderSource, char *fragmentShaderSource) {
	int status, logLen;
	char *infoLog;

	int vertexShader;
	int geometryShader;
	int fragmentShader;
	int shaderProgram;

	const char * const* vs = (const char * const*) &vertexShaderSource;
	const char * const* gs = (const char * const*) &geometryShaderSource;
	const char * const* fs = (const char * const*) &fragmentShaderSource;

	if(vertexShaderSource) {
		vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, vs, NULL);
		glCompileShader(vertexShader);
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);

		if(!status) {
			glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLen);
			infoLog = malloc(sizeof(char) * logLen);
			glGetShaderInfoLog(vertexShader, logLen, NULL, infoLog);
			printf("Error while compiling vertex shader:\n%s", infoLog);
			free(infoLog);
		}
	}

	if(geometryShaderSource) {
		geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geometryShader, 1, gs, NULL);
		glCompileShader(geometryShader);
		glGetShaderiv(geometryShader, GL_COMPILE_STATUS, &status);

		if(!status) {
			glGetShaderiv(geometryShader, GL_INFO_LOG_LENGTH, &logLen);
			infoLog = malloc(sizeof(char) * logLen);
			glGetShaderInfoLog(geometryShader, logLen, NULL, infoLog);
			printf("Error while compiling geometry shader:\n%s", infoLog);
			free(infoLog);
		}
	}

	if(fragmentShaderSource) {
		fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, fs, NULL);
		glCompileShader(fragmentShader);
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);

		if(!status) {
			glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &logLen);
			infoLog = malloc(sizeof(char) * logLen);
			glGetShaderInfoLog(fragmentShader, logLen, NULL, infoLog);
			printf("Error while compiling fragment shader:\n%s", infoLog);
			free(infoLog);
		}
	}

	shaderProgram = glCreateProgram();
	if(vertexShaderSource) glAttachShader(shaderProgram, vertexShader);
	if(geometryShaderSource) glAttachShader(shaderProgram, geometryShader);
	if(fragmentShaderSource) glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);

	if(!status) {
		glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLen);
		infoLog = malloc(sizeof(char) * logLen);
		glGetProgramInfoLog(shaderProgram, logLen, NULL, infoLog);
		printf("Error while linking shader program:\n%s", infoLog);
		free(infoLog);
	}

	if(vertexShaderSource) glDeleteShader(vertexShader);
	if(geometryShaderSource) glDeleteShader(geometryShader);
	if(fragmentShaderSource) glDeleteShader(fragmentShader);

	return shaderProgram;
}
