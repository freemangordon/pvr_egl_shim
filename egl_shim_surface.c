/*
 * egl_shim_surface.c
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

#include <stdlib.h>
#include <stdio.h>
#include <gbm.h>

#include "egl_shim_surface.h"
#include "list.h"

EGLShimSurface *
egl_shim_surface_create(EGLNativeWindowType win)
{
  EGLShimSurface *surf = calloc(sizeof(EGLShimSurface), 1);

  surf->drawable = (xcb_drawable_t)win;

  return surf;
}

void
egl_shim_surface_destroy(EGLShimSurface *surf)
{
  if (surf->gbm_surface)
    gbm_surface_destroy(surf->gbm_surface);

  free(surf);
}

EGLShimSurfaceList
egl_shim_surface_list_append(EGLShimSurfaceList list, EGLShimSurface *surf)
{
  return slist_append(list, surf);
}

static int
match_egl_surface(const void *data, const void *user_data)
{
  const EGLShimSurface *surf = data;
  const EGLSurface egl_surface = (const EGLSurface)user_data;

  if (surf->egl_surface == egl_surface)
    return 1;

  return 0;
}

EGLShimSurface *
egl_shim_surface_list_find_by_egl_surface(EGLShimSurfaceList list,
                                          EGLSurface egl_surface)
{
  slist *l = slist_find(list, match_egl_surface, egl_surface);

  if (l)
    return l->data;

  return NULL;
}

static int
match_drawable(const void *data, const void *user_data)
{
  const EGLShimSurface *surf = data;
  xcb_drawable_t drawable = (xcb_drawable_t)user_data;

  if (surf->drawable == drawable)
    return 1;

  return 0;
}

EGLShimSurface *
egl_shim_surface_list_find_by_drawable(EGLShimSurfaceList list,
                                       xcb_drawable_t drawable)
{
  slist *l = slist_find(list, match_drawable, (void *)drawable);

  if (l)
    return l->data;

  return NULL;
}

static int
match_pixmap(const void *data, const void *user_data)
{
  const EGLPixmapBuffer *pb = data;
  xcb_pixmap_t pixmap = (xcb_pixmap_t)user_data;

  if (pb->pixmap == pixmap)
    return 1;

  return 0;
}

EGLPixmapBuffer *
egl_surface_pixmap_buffer_find_by_pixmap(EGLShimSurface *surf,
                                         xcb_pixmap_t pixmap)
{
  slist *l = slist_find(surf->buffers, match_pixmap, (void *)pixmap);

  if (l)
    return l->data;

  return NULL;
}

static int
match_serial(const void *data, const void *user_data)
{
  const EGLPixmapBuffer *pb = data;
  uint32_t serial = (uintptr_t)user_data;

  if (pb->serial == serial)
    return 1;

  return 0;
}

EGLPixmapBuffer *
egl_surface_pixmap_buffer_find_by_serial(EGLShimSurface *surf,
                                         uint32_t serial)
{
  slist *l = slist_find(surf->buffers, match_serial, (void *)(uintptr_t)serial);

  if (l)
    return l->data;

  return NULL;
}
