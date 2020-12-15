/*
 * xcb_pixmap.h
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

#ifndef EGL_PIXMAP_H
#define EGL_PIXMAP_H

#include <gbm.h>
#include <xcb/xcb.h>

#include "defs.h"
#include "egl_shim_display.h"

struct _EGLPixmapBuffer
{
  EGLShimSurface *surf;
  struct gbm_bo *bo;
  xcb_pixmap_t pixmap;
  Bool busy;
  uint32_t serial;
};

EGLPixmapBuffer *
egl_pixmap_buffer_create(EGLShimDisplay *dpy, EGLShimSurface *surf,
                         struct gbm_bo *bo);
void
egl_pixmap_buffer_present(EGLShimDisplay *dpy, EGLShimSurface *surf,
                          EGLPixmapBuffer *pb, uint32_t options);

#endif // EGL_PIXMAP_H
