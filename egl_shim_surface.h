/*
 * egl_shim_surface.h
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

#ifndef EGL_SHIM_SURFACE_H
#define EGL_SHIM_SURFACE_H

#include <EGL/egl.h>
#include <gbm.h>
#include <xcb/xcb.h>

#include "egl_pixmap.h"

struct _EGLShimSurface
{
  EGLSurface egl_surface;
  xcb_drawable_t drawable;
  struct gbm_surface *gbm_surface;
  xcb_special_event_t *ev;
  int bpp;
  EGLShimPixmapBufferList buffers;
  uint32_t buf_count;
  uint32_t lock_count;
};

EGLShimSurface *
egl_shim_surface_create(EGLNativeWindowType win);

void
egl_shim_surface_destroy(EGLShimSurface *surf);

__attribute__((warn_unused_result)) EGLShimSurfaceList
egl_shim_surface_list_append(EGLShimSurfaceList list, EGLShimSurface *surf);

EGLShimSurface *
egl_shim_surface_list_find_by_egl_surface(EGLShimSurfaceList list,
                                          EGLSurface egl_surface);

EGLShimSurface *
egl_shim_surface_list_find_by_drawable(EGLShimSurfaceList list,
                                       xcb_drawable_t drawable);

EGLPixmapBuffer *
egl_surface_pixmap_buffer_find_by_pixmap(EGLShimSurface *surf,
                                         xcb_pixmap_t pixmap);

EGLPixmapBuffer *
egl_surface_pixmap_buffer_find_by_serial(EGLShimSurface *surf,
                                         uint32_t serial);

void
egl_surface_pixmap_buffers_release(EGLShimSurface *surf);

#endif // EGL_SHIM_SURFACE_H
