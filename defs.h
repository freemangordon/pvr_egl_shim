/*
 * defs.h
 *
 * Copyright (C) 2020 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef DEFS_H
#define DEFS_H

#define Bool int
#define Status int
#define True 1
#define False 0

typedef struct _EGLPixmapBuffer EGLPixmapBuffer;
typedef struct _EGLShimSurface EGLShimSurface;
typedef struct _EGLShimDisplay EGLShimDisplay;
typedef void * EGLShimSurfaceList;
typedef void * EGLShimPixmapBufferList;

// #define DBG 1

#ifdef DBG
#define DEBUG(format, ...) printf (format, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

#endif // DEFS_H
