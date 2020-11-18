#define _GNU_SOURCE

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <dlfcn.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <gbm.h>

#include <X11/Xutil.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <X11/Xlib-xcb.h>

#include "egl_pvr.h"

static EGLAPI EGLDisplay EGLAPIENTRY (*_eglGetDisplay)(EGLNativeDisplayType display_id) = 0;
static EGLAPI EGLBoolean EGLAPIENTRY (*_eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
static EGLAPI EGLSurface EGLAPIENTRY (*_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) = 0;
static EGLAPI EGLBoolean EGLAPIENTRY (*_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

static struct gbm_device *gbm = 0;
static int dri3_fd = -1;
Display *x_dpy = NULL;

static void
x_check_dri3_ext(xcb_connection_t *xcb_conn, xcb_screen_t *screen)
{
  xcb_prefetch_extension_data (xcb_conn, &xcb_dri3_id);

  const xcb_query_extension_reply_t *extension =
      xcb_get_extension_data(xcb_conn, &xcb_dri3_id);

  if (!(extension && extension->present))
  {
    printf("No DRI3 support\n");
    return;
  }

  xcb_dri3_query_version_cookie_t cookie =
      xcb_dri3_query_version(xcb_conn, XCB_DRI3_MAJOR_VERSION,
                             XCB_DRI3_MINOR_VERSION);
  xcb_dri3_query_version_reply_t *reply =
      xcb_dri3_query_version_reply(xcb_conn, cookie, NULL);

  if (!reply)
  {
    printf("xcb_dri3_query_version failed\n");
    return;
  }

  printf("DRI3 %u.%u\n", reply->major_version, reply->minor_version);

  free(reply);
}

static int
x_dri3_open(xcb_connection_t *xcb_conn, xcb_screen_t *screen)
{
  xcb_dri3_open_cookie_t cookie = xcb_dri3_open(xcb_conn, screen->root, 0);
  xcb_dri3_open_reply_t *reply = xcb_dri3_open_reply(xcb_conn, cookie, NULL);

  if (!reply)
  {
    printf("dri3 open failed\n");
    return -1;
  }

  int nfds = reply->nfd;

  if (nfds != 1)
  {
    printf("bad number of fds\n");
    return -1;
  }

  int *fds = xcb_dri3_open_reply_fds(xcb_conn, reply);

  int fd = fds[0];

  free(reply);

  fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

  printf("DRM FD %d\n", fd);

  return fd;
}

static void
x_check_present_ext(xcb_connection_t *xcb_conn, xcb_screen_t *screen)
{
  xcb_prefetch_extension_data (xcb_conn, &xcb_present_id);

  const xcb_query_extension_reply_t *extension =
      xcb_get_extension_data(xcb_conn, &xcb_present_id);

  if (!(extension && extension->present))
  {
    printf("No present extension\n");
    return;
  }

  xcb_present_query_version_cookie_t cookie =
      xcb_present_query_version(xcb_conn, XCB_PRESENT_MAJOR_VERSION,
                                XCB_PRESENT_MINOR_VERSION);
  xcb_present_query_version_reply_t *reply =
      xcb_present_query_version_reply(xcb_conn, cookie, NULL);

  if (!reply)
  {
    printf("xcb_present_query_version failed\n");
    return;
  }

  printf("present %u.%u\n", reply->major_version, reply->minor_version);
  free(reply);
}

static EGLBoolean
init(Display *dpy)
{
  static int init_done = 0;

  if (init_done)
    return EGL_TRUE;

  xcb_connection_t *xcb_conn = XGetXCBConnection(dpy);
  const xcb_setup_t *setup = xcb_get_setup(xcb_conn);

  if (!setup)
  {
    printf("xcb_get_setup failed\n");
    return EGL_FALSE;
  }

  /* get the first screen */
  xcb_screen_t *screen = xcb_setup_roots_iterator(setup).data;

  if (!screen)
  {
    printf("failed to find screen\n");
    return EGL_FALSE;
  }

  x_check_dri3_ext(xcb_conn, screen);
  x_check_present_ext(xcb_conn, screen);
  dri3_fd = x_dri3_open(xcb_conn, screen);

  if (dri3_fd == -1)
    return EGL_FALSE;

  gbm = gbm_create_device(dri3_fd);

  if (!gbm)
  {
    printf("cannot creategbm device\n");
    return EGL_FALSE;
  }

  _eglGetDisplay = dlsym(RTLD_NEXT, "eglGetDisplay");
  _eglGetConfigAttrib = dlsym(RTLD_NEXT, "eglGetConfigAttrib");
  _eglCreateWindowSurface = dlsym(RTLD_NEXT, "eglCreateWindowSurface");
  _eglSwapBuffers = dlsym(RTLD_NEXT, "eglSwapBuffers");

  init_done = 1;

  return EGL_TRUE;
}

EGLAPI EGLDisplay EGLAPIENTRY
eglGetDisplay(EGLNativeDisplayType display_id)
{
  x_dpy = display_id;
  init(display_id);
  printf("_eglGetDisplay\n");

  return _eglGetDisplay((EGLNativeDisplayType)gbm);
}

EGLAPI EGLBoolean EGLAPIENTRY
eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute,
                   EGLint *value)
{
  XVisualInfo vinfo;

  if (attribute != EGL_NATIVE_VISUAL_ID)
    return _eglGetConfigAttrib(dpy, config, attribute, value);

  init(x_dpy);

  if (!XMatchVisualInfo(x_dpy,  DefaultScreen(x_dpy), 24, TrueColor, &vinfo))
    return EGL_FALSE;

  *value = vinfo.visualid;

  return EGL_TRUE;
}

static struct gbm_surface *gbm_surface = 0;
static EGLNativeWindowType _win;
static xcb_special_event_t *special_ev = NULL;

xcb_special_event_t *x_init_special_event_queue(xcb_connection_t *c, xcb_window_t window, uint32_t *special_ev_stamp)
{
  uint32_t id = xcb_generate_id(c);
  xcb_void_cookie_t cookie;

  cookie = xcb_present_select_input_checked(c, id, window,
                                            XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY |
                                            XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY |
                                            XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY/* |
                                                                                            XCB_PRESENT_EVENT_MASK_REDIRECT_NOTIFY*/);
  xcb_generic_error_t *error =
      xcb_request_check(c, cookie);
  if (error)
  {
    printf("req xge failed\n");
    return NULL;
  }

  xcb_special_event_t *special_ev = xcb_register_for_special_xge(c, &xcb_present_id, id, special_ev_stamp);

  if (!special_ev)
    printf("no special ev\n");

  return special_ev;
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                       EGLNativeWindowType win, const EGLint *attrib_list)
{

  struct dri2_egl_config *c = (struct dri2_egl_config *)config;
  EGLSurface egl_surface;
  xcb_connection_t *xcb_conn = XGetXCBConnection(x_dpy);
  xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry(xcb_conn, win);
  xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(xcb_conn,
                                                          geomCookie, NULL);

  _win = win;

  gbm_surface = gbm_surface_create(gbm, geom->width, geom->height,
                                   c->base.NativeVisualID,
                                   GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);

  free(geom);

  special_ev = x_init_special_event_queue(xcb_conn, win, NULL);

  egl_surface = _eglCreateWindowSurface(dpy, config,
                                        (EGLNativeWindowType)gbm_surface,
                                        attrib_list);

  return egl_surface;
}

enum buffer_status
{
  BUFFER_STATUS_FREE,
  BUFFER_STATUS_PENDING,
  BUFFER_STATUS_FINISHED
};

struct gbm_bo_buffer
{
  struct gbm_bo *bo;
  xcb_pixmap_t pixmap;
  enum buffer_status status;
  struct gbm_bo_buffer *next;
};

static struct gbm_bo_buffer *buffers = NULL;

static void
destroy_gbm_bo_buffer(struct gbm_bo *bo, void *data)
{
  struct gbm_bo_buffer *b = buffers;
  struct gbm_bo_buffer *prev = buffers;
  struct gbm_bo_buffer *next = buffers;

  while (b && b != data)
  {
    prev = b;
    b = b->next;
    next = b;
  }

  if (prev)
    prev->next = next;

  free(data);
}

static int
release_gbm_bo_buffers()
{
  struct gbm_bo_buffer *b = buffers;
  int released = 0;

  while (b)
  {
    if (b->status == BUFFER_STATUS_FINISHED)
    {
      gbm_surface_release_buffer(gbm_surface, b->bo);
      b->status = BUFFER_STATUS_FREE;
      released = 1;
    }

    b = b->next;
  }

  return released;
}

static struct gbm_bo_buffer *
create_gbm_bo_buffer(xcb_connection_t *xcb_conn, struct gbm_bo *bo)
{
  struct gbm_bo_buffer *b;
  struct gbm_bo_buffer *new_b = calloc(sizeof(struct gbm_bo_buffer), 1);

  if (!buffers)
    buffers = new_b;
  else
  {
    b = buffers;

    while (b->next)
      b = b->next;

    b->next = new_b;
  }

  b = new_b;

  xcb_pixmap_t pixmap = xcb_generate_id(xcb_conn);
  xcb_void_cookie_t pixmap_cookie =
      xcb_dri3_pixmap_from_buffer_checked(xcb_conn, pixmap, _win, 0,
                                          gbm_bo_get_width(bo),
                                          gbm_bo_get_height(bo),
                                          gbm_bo_get_stride(bo),
                                          24,
                                          gbm_bo_get_bpp(bo),
                                          gbm_bo_get_fd(bo));
  xcb_generic_error_t *error;

  if ((error = xcb_request_check(xcb_conn, pixmap_cookie)))
    printf("create pixmap failed\n");

  b->pixmap = pixmap;
  b->bo = bo;

  gbm_bo_set_user_data(bo, b, destroy_gbm_bo_buffer);

  return b;
}

static struct gbm_bo_buffer *
find_gbm_bo_buffer(xcb_pixmap_t pixmap)
{
  struct gbm_bo_buffer *b = buffers;

  while (b && b->pixmap != pixmap)
    b = b->next;

  return b;
}

static void
handle_special_event(xcb_present_generic_event_t *ge)
{
  switch (ge->evtype)
  {
    case XCB_PRESENT_COMPLETE_NOTIFY:
    {
      /*
      xcb_present_complete_notify_event_t *ce = (xcb_present_complete_notify_event_t*) ge;


      DBG("XCB_PRESENT_COMPLETE_NOTIFY %u, %s, msc %llu, ust %llu", ce->serial,
          get_complete_mode_str(ce->mode),
          (unsigned long long)ce->msc, (unsigned long long)ce->ust);
      */
      break;
    }

    case XCB_PRESENT_EVENT_IDLE_NOTIFY:
    {
      xcb_present_idle_notify_event_t *ie =
          (xcb_present_idle_notify_event_t *)ge;
      struct gbm_bo_buffer *b = find_gbm_bo_buffer(ie->pixmap);

      assert(b->status == BUFFER_STATUS_PENDING);
      if (b)
        b->status = BUFFER_STATUS_FINISHED;

      break;
    }

    case XCB_PRESENT_EVENT_CONFIGURE_NOTIFY:
    {
      /*
      __attribute__((unused))
          xcb_present_configure_notify_event_t *ce = (xcb_present_configure_notify_event_t*) ge;
      drawable->size_changed = true;
      */
      break;
    }

    case XCB_PRESENT_EVENT_REDIRECT_NOTIFY:
    {
      /*
      __attribute__((unused))
          xcb_present_redirect_notify_event_t *re = (xcb_present_redirect_notify_event_t*) ge;
      DBG("XCB_PRESENT_EVENT_REDIRECT_NOTIFY %u", re->serial);
     */
      break;
    }

  }

  free(ge);
}
static void
poll_special_events(struct xcb_connection_t *xcb_conn)
{
  xcb_generic_event_t *ev;

  while ((ev = xcb_poll_for_special_event(xcb_conn, special_ev)) != NULL)
  {
    xcb_present_generic_event_t *ge = (void *)ev;
    handle_special_event(ge);
  }
}

static void
wait_special_event(struct xcb_connection_t *xcb_conn)
{
  xcb_generic_event_t *ev;

  ev = xcb_wait_for_special_event(xcb_conn, special_ev);
  handle_special_event((xcb_present_generic_event_t*)ev);

  poll_special_events(xcb_conn);
}

EGLAPI EGLBoolean EGLAPIENTRY
eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
  if (!_eglSwapBuffers(dpy, surface))
    return EGL_FALSE;

  xcb_connection_t *xcb_conn = XGetXCBConnection(x_dpy);

  poll_special_events(xcb_conn);

  struct gbm_bo *bo = gbm_surface_lock_front_buffer(gbm_surface);
  struct gbm_bo_buffer *b = gbm_bo_get_user_data(bo);

  xcb_generic_error_t *error;
  uint32_t options = XCB_PRESENT_OPTION_NONE;

  if (!b)
    b = create_gbm_bo_buffer(xcb_conn, bo);

  assert(b->status == BUFFER_STATUS_FREE);

  b->status = BUFFER_STATUS_PENDING;

  xcb_void_cookie_t cookie = xcb_present_pixmap_checked(xcb_conn,
                                                        _win,
                                                        b->pixmap,
                                                        0,
                                                        0, 0, 0, 0, // valid, update, x_off, y_off
                                                        None, /* target_crtc */
                                                        None, /* wait fence */
                                                        None, /* idle fence */
                                                        options,
                                                        0,
                                                        0, /* divisor */
                                                        0, /* remainder */
                                                        0, /* notifiers len */
                                                        NULL); /* notifiers */

  if ((error = xcb_request_check(xcb_conn, cookie)))
    printf("present pixmap failed\n");

  xcb_flush(xcb_conn);

  release_gbm_bo_buffers();

  while (!gbm_surface_has_free_buffers(gbm_surface))
  {
    wait_special_event(xcb_conn);
    release_gbm_bo_buffers();
  }

  return EGL_TRUE;
}
