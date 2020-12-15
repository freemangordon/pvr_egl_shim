/*
 * xcb_dri3.h
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

#ifndef XCB_DRI3_H
#define XCB_DRI3_H

#include <gbm.h>

#include "defs.h"

struct gbm_device *
xcb_dri3_create_gbm_device(xcb_connection_t *xcb_conn, xcb_screen_t *screen);


#endif // XCB_DRI3_H
