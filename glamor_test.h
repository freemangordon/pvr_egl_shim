/*
 * glamor_test.h
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

#ifndef GLAMOR_TEST_H
#define GLAMOR_TEST_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define CMD_PIXMAP_CREATE '1'
#define CMD_PIXMAP_PRESENT '2'

#define hexstrntoul(s) _hexstrntoul(s, sizeof(s))

static inline uint32_t
_hexstrntoul(const char *s, int nchars)
{
  char tmp[nchars + 1];

  memcpy(tmp, s, nchars);
  tmp[nchars] = 0;

  return strtoul(tmp, NULL, 16);
}

struct msg
{
  char cmd;
  union
  {
    struct
    {
      char id[8];
      char width[8];
      char height[8];
      char stride[8];
      char format[8];
    } create;
    struct
    {
      char id[8];
    } present;
  } u;
};

#define SOCKET_NAME "glamor"

ssize_t write_fd(int sock, void *buf, ssize_t buflen, int fd);
ssize_t read_fd(int sock, void *buf, ssize_t bufsize, int *fd);
int bind_named_socket(const char *filename);
int connect_named_socket(const char *filename);

#endif // GLAMOR_TEST_H
