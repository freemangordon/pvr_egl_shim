/*
 * xcb_pixmap.c
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

#include <xcb/dri3.h>
#include <xcb/present.h>

#include <stdlib.h>
#include <stdio.h>

#include "egl_pixmap.h"
#include "list.h"

void
egl_pixmap_buffer_destroy(struct gbm_bo *bo, void *data)
{
  EGLPixmapBuffer *pb = data;

  pb->surf->buffers = slist_remove(pb->surf->buffers, pb, free);
}

EGLPixmapBuffer *
egl_pixmap_buffer_create(EGLShimDisplay *dpy, EGLShimSurface *surf,
                         struct gbm_bo *bo)
{
  EGLPixmapBuffer *pb = calloc(sizeof(EGLPixmapBuffer), 1);
  xcb_pixmap_t pixmap = xcb_generate_id(dpy->xcb_conn);
  xcb_generic_error_t *error;
  xcb_void_cookie_t pixmap_cookie;

  pixmap_cookie =
      xcb_dri3_pixmap_from_buffer_checked(dpy->xcb_conn,
                                          pixmap,
                                          surf->drawable,
                                          0,
                                          gbm_bo_get_width(bo),
                                          gbm_bo_get_height(bo),
                                          gbm_bo_get_stride(bo),
                                          surf->bpp,
                                          gbm_bo_get_bpp(bo),
                                          gbm_bo_get_fd(bo));

  if ((error = xcb_request_check(dpy->xcb_conn, pixmap_cookie)))
  {
    fprintf(stderr, "create pixmap failed\n");
    free(pb);
    return NULL;
  }

  pb->pixmap = pixmap;
  pb->bo = bo;
  pb->surf = surf;
  pb->serial = surf->buf_count++;

  gbm_bo_set_user_data(bo, pb, egl_pixmap_buffer_destroy);

  surf->buffers = slist_append(surf->buffers, pb);

  return pb;
}

void
egl_pixmap_buffer_present(EGLShimDisplay *dpy, EGLShimSurface *surf,
                          EGLPixmapBuffer *pb)
{
  xcb_void_cookie_t cookie;
  xcb_generic_error_t *error;
  uint32_t options = XCB_PRESENT_OPTION_ASYNC;

  cookie = xcb_present_pixmap_checked(dpy->xcb_conn, surf->drawable, pb->pixmap,
                                      pb->serial,
                                      0, 0, 0, 0, // valid, update, x_off, y_off
                                      None, /* target_crtc */
                                      None, /* wait fence */
                                      None, /* idle fence */
                                      options,
                                      0,
                                      surf->buf_count, /* divisor */
                                      pb->serial, /* remainder */
                                      0, /* notifiers len */
                                      NULL); /* notifiers */

  if ((error = xcb_request_check(dpy->xcb_conn, cookie)))
    fprintf(stderr, "present pixmap failed %d\n", error->error_code);

  xcb_flush(dpy->xcb_conn);

  //DEBUG("presented %dn", pb->serial);
}
