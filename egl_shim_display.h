/*
 * egl_shim_display.h
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

#ifndef EGL_SHIM_DISPLAY_H
#define EGL_SHIM_DISPLAY_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <X11/Xutil.h>
#include <xcb/xcb.h>
#include <gbm.h>

#include "egl_shim_surface.h"

struct _EGLShimDisplay
{
  Display *x_dpy;
  xcb_connection_t *xcb_conn;
  xcb_special_event_t *special_ev;
  struct gbm_device *gbm;
  EGLDisplay egl_dpy;
  EGLShimSurfaceList surfaces;
};

EGLShimDisplay *
egl_shim_display_create(Display *x_dpy);

EGLShimDisplay *
egl_shim_display_find(EGLDisplay egl_dpy);

void
egl_shim_display_remove(EGLShimDisplay *dpy);

EGLShimSurface *
egl_shim_display_create_surface(EGLShimDisplay *dpy, EGLNativeWindowType win,
                                uint32_t format);
void
egl_shim_display_add_surface(EGLShimDisplay *dpy, EGLShimSurface *surf);

EGLShimSurface *
egl_shim_display_find_surface(EGLShimDisplay *dpy, EGLSurface egl_surface);

#endif // EGL_SHIM_DISPLAY_H
