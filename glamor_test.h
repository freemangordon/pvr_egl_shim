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
      char fd[8];
    } create;
    struct
    {
      char id[8];
    } present;
  } u;
};

#endif // GLAMOR_TEST_H
