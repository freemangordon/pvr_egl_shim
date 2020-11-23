/*
 * egl_shim_display.c
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

#include <xcb/xcb.h>
#include <X11/Xlib-xcb.h>
#include <stdlib.h>
#include <stdio.h>

#include "egl_shim_display.h"
#include "xcb_dri3.h"
#include "xcb_event.h"
#include "list.h"

static slist *egl_displays = NULL;

EGLShimDisplay *
egl_shim_display_create(Display *x_dpy)
{
  EGLShimDisplay *dpy = calloc(sizeof(EGLShimDisplay), 1);
  const xcb_setup_t *setup;
  xcb_screen_t *screen;

  dpy->x_dpy = x_dpy;
  dpy->xcb_conn = XGetXCBConnection(x_dpy);

  if (!(setup = xcb_get_setup(dpy->xcb_conn)))
  {
    fprintf(stderr, "xcb_get_setup failed\n");
    goto fail;
  }

  /* get the first screen */
  if (!(screen = xcb_setup_roots_iterator(setup).data))
  {
    fprintf(stderr, "failed to find screen\n");
    goto fail;
  }

  if (!(dpy->gbm = xcb_dri3_create_gbm_device(dpy->xcb_conn, screen)))
      goto fail;

  egl_displays = slist_append(egl_displays, dpy);

  return dpy;

fail:
  free(dpy);

  return NULL;
}

static int
egl_display_find(const void *data, const void *user_data)
{
  const EGLShimDisplay *dpy1 = data;
  const EGLDisplay egl_dpy = (const EGLDisplay)user_data;

  if (dpy1->egl_dpy == egl_dpy)
    return 1;

  return 0;
}

EGLShimDisplay *
egl_shim_display_find(EGLDisplay egl_dpy)
{
  slist *l = slist_find(egl_displays, egl_display_find, egl_dpy);

  if (l)
    return l->data;

  return NULL;
}

static void
egl_shim_display_destroy(void * data)
{
  EGLShimDisplay *dpy = data;

  gbm_device_destroy(dpy->gbm);

  free(data);
}

void
egl_shim_display_remove(EGLShimDisplay *dpy)
{
  egl_displays = slist_remove(egl_displays, dpy, egl_shim_display_destroy);
}

EGLShimSurface *
egl_shim_display_create_surface(EGLShimDisplay *dpy, EGLNativeWindowType win,
                                uint32_t format)
{
  EGLShimSurface *surf = egl_shim_surface_create(win);
  xcb_get_geometry_cookie_t cookie = xcb_get_geometry(dpy->xcb_conn, win);
  xcb_get_geometry_reply_t *geom =
      xcb_get_geometry_reply(dpy->xcb_conn, cookie, NULL);

  surf->gbm_surface =
      gbm_surface_create(dpy->gbm, geom->width, geom->height,format,
                         GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);

  free(geom);

  if (!surf->gbm_surface)
  {
    fprintf(stdout, "Unable to create gbm surface");
    goto fail;
  }

  surf->ev = xcb_event_init_special_event_queue(dpy->xcb_conn, win, NULL);

  return surf;

fail:

  egl_shim_surface_destroy(surf);

  return NULL;
}

void
egl_shim_display_add_surface(EGLShimDisplay *dpy, EGLShimSurface *surf)
{
  dpy->surfaces = egl_shim_surface_list_append(dpy->surfaces, surf);
}

EGLShimSurface *
egl_shim_display_find_surface(EGLShimDisplay *dpy, EGLSurface egl_surface)
{
  return egl_shim_surface_list_find_by_egl_surface(dpy->surfaces, egl_surface);
}
