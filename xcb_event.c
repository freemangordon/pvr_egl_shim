/*
 * xcb_event.c
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

#include <stdio.h>

#include "xcb_event.h"

xcb_special_event_t *
xcb_event_init_special_event_queue(xcb_connection_t *c, xcb_window_t window,
                                   uint32_t *special_ev_stamp)
{
  uint32_t id = xcb_generate_id(c);
  xcb_generic_error_t *error;
  xcb_void_cookie_t cookie;
  xcb_special_event_t *special_ev;

  cookie = xcb_present_select_input_checked(
        c, id, window,
        XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY |
        XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY |
        XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY /*|
        XCB_PRESENT_EVENT_MASK_REDIRECT_NOTIFY*/);

  error = xcb_request_check(c, cookie);

  if (error)
  {
    fprintf(stderr, "req xge failed\n");
    return NULL;
  }

  special_ev = xcb_register_for_special_xge(c, &xcb_present_id, id,
                                            special_ev_stamp);

  if (!special_ev)
    fprintf(stderr, "no special ev\n");

  return special_ev;
}
