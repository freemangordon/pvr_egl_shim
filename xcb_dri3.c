/*
 * xcb_dri3.c
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "xcb_dri3.h"

static Bool
xcb_check_dri3_ext(xcb_connection_t *xcb_conn, xcb_screen_t *screen)
{
  xcb_prefetch_extension_data (xcb_conn, &xcb_dri3_id);

  const xcb_query_extension_reply_t *extension =
      xcb_get_extension_data(xcb_conn, &xcb_dri3_id);

  if (!(extension && extension->present))
  {
    fprintf(stderr, "No DRI3 support\n");
    return False;
  }

  xcb_dri3_query_version_cookie_t cookie =
      xcb_dri3_query_version(xcb_conn, XCB_DRI3_MAJOR_VERSION,
                             XCB_DRI3_MINOR_VERSION);
  xcb_dri3_query_version_reply_t *reply =
      xcb_dri3_query_version_reply(xcb_conn, cookie, NULL);

  if (!reply)
  {
    fprintf(stderr, "xcb_dri3_query_version failed\n");
    return False;
  }

  printf("DRI3 %u.%u\n", reply->major_version, reply->minor_version);

  free(reply);

  return True;
}

static int
xcb_dri3_open_dri3(xcb_connection_t *xcb_conn, xcb_screen_t *screen)
{
  xcb_dri3_open_cookie_t cookie = xcb_dri3_open(xcb_conn, screen->root, 0);
  xcb_dri3_open_reply_t *reply = xcb_dri3_open_reply(xcb_conn, cookie, NULL);

  if (!reply)
  {
    fprintf(stderr, "dri3 open failed\n");
    return -1;
  }

  int nfds = reply->nfd;

  if (nfds != 1)
  {
    fprintf(stderr, "bad number of fds\n");
    return -1;
  }

  int *fds = xcb_dri3_open_reply_fds(xcb_conn, reply);

  int fd = fds[0];

  free(reply);

  fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

  return fd;
}

static Bool
xcb_check_present_ext(xcb_connection_t *xcb_conn, xcb_screen_t *screen)
{
  xcb_prefetch_extension_data (xcb_conn, &xcb_present_id);

  const xcb_query_extension_reply_t *extension =
      xcb_get_extension_data(xcb_conn, &xcb_present_id);

  if (!(extension && extension->present))
  {
    fprintf(stderr, "No present extension\n");
    return False;
  }

  xcb_present_query_version_cookie_t cookie =
      xcb_present_query_version(xcb_conn, XCB_PRESENT_MAJOR_VERSION,
                                XCB_PRESENT_MINOR_VERSION);
  xcb_present_query_version_reply_t *reply =
      xcb_present_query_version_reply(xcb_conn, cookie, NULL);

  if (!reply)
  {
    fprintf(stderr, "xcb_present_query_version failed\n");
    return False;
  }

  free(reply);

  return True;
}

struct gbm_device *
xcb_dri3_create_gbm_device(xcb_connection_t *xcb_conn, xcb_screen_t *screen)
{
  int dri3_fd;

  if (xcb_check_dri3_ext(xcb_conn, screen) &&
      xcb_check_present_ext(xcb_conn, screen) &&
      (dri3_fd = xcb_dri3_open_dri3(xcb_conn, screen)) != -1)
  {
    return gbm_create_device(dri3_fd);
  }

  return NULL;
}
