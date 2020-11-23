#define _GNU_SOURCE

#include <dlfcn.h>
#include <xcb/xcb.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "egl_shim_display.h"
#include "egl_pixmap.h"
#include "xcb_event.h"

#include "egl_pvr.h"

static EGLAPI EGLDisplay EGLAPIENTRY (*_eglGetDisplay)(EGLNativeDisplayType display_id) = 0;
static EGLAPI EGLBoolean EGLAPIENTRY (*_eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
static EGLAPI EGLSurface EGLAPIENTRY (*_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) = 0;
static EGLAPI EGLBoolean EGLAPIENTRY (*_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

static int
bpp_from_gbm_fourcc(uint32_t gbm_fourcc)
{
  switch (gbm_fourcc)
  {
    case GBM_FORMAT_RGB888:
    case GBM_FORMAT_BGR888:
    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_XBGR8888:
    case GBM_FORMAT_RGBX8888:
    case GBM_FORMAT_BGRX8888:
    {
      return 24;
    }
    case GBM_FORMAT_ARGB8888:
    case GBM_FORMAT_ABGR8888:
    case GBM_FORMAT_RGBA8888:
    case GBM_FORMAT_BGRA8888:
    {
      return 32;
    }
    default:
      assert(0);
  }

  return 0;
}

static void
hook_egl_functions()
{
  static int init_done = 0;

  if (!init_done)
  {
    _eglGetDisplay = dlsym(RTLD_NEXT, "eglGetDisplay");
    _eglGetConfigAttrib = dlsym(RTLD_NEXT, "eglGetConfigAttrib");
    _eglCreateWindowSurface = dlsym(RTLD_NEXT, "eglCreateWindowSurface");
    _eglSwapBuffers = dlsym(RTLD_NEXT, "eglSwapBuffers");

    init_done = 1;
  }
}

EGLAPI EGLDisplay EGLAPIENTRY
eglGetDisplay(EGLNativeDisplayType display_id)
{
  EGLShimDisplay *dpy;
  EGLDisplay egl_dpy;

  hook_egl_functions();

  dpy = egl_shim_display_create(display_id);

  if (!dpy)
    return NULL;

  egl_dpy = _eglGetDisplay((EGLNativeDisplayType)dpy->gbm);
  dpy->egl_dpy = egl_dpy;

  if (!egl_dpy)
    egl_shim_display_remove(dpy);

  return egl_dpy;
}

EGLAPI EGLBoolean EGLAPIENTRY
eglGetConfigAttrib(EGLDisplay egl_dpy, EGLConfig config, EGLint attribute,
                   EGLint *value)
{
  XVisualInfo vinfo;
  EGLShimDisplay *dpy;
  Display *x_dpy;
  int bpp;

  hook_egl_functions();

  if (attribute != EGL_NATIVE_VISUAL_ID)
    return _eglGetConfigAttrib(egl_dpy, config, attribute, value);

  dpy = egl_shim_display_find(egl_dpy);

  assert(dpy);

  bpp = bpp_from_gbm_fourcc(((_EGLConfig *)config)->NativeVisualID);
  x_dpy = dpy->x_dpy;

  if (!XMatchVisualInfo(x_dpy,  DefaultScreen(x_dpy), bpp, TrueColor, &vinfo))
    return EGL_FALSE;

  *value = vinfo.visualid;

  return EGL_TRUE;
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreateWindowSurface(EGLDisplay egl_dpy, EGLConfig config,
                       EGLNativeWindowType win, const EGLint *attrib_list)
{
  EGLShimDisplay *dpy = egl_shim_display_find(egl_dpy);
  uint32_t gbm_fourcc = ((struct _egl_config *)config)->NativeVisualID;
  EGLSurface egl_surface;
  EGLShimSurface *surf;

  hook_egl_functions();

  assert(dpy);

  surf = egl_shim_display_create_surface(dpy, win, gbm_fourcc);

  if (!surf)
    return NULL;

  surf->bpp = bpp_from_gbm_fourcc(gbm_fourcc);

  egl_surface = _eglCreateWindowSurface(egl_dpy, config,
                                        (EGLNativeWindowType)surf->gbm_surface,
                                        attrib_list);

  if (egl_surface)
  {
    surf->egl_surface = egl_surface;
    egl_shim_display_add_surface(dpy, surf);
  }
  else
    egl_shim_surface_destroy(surf);

  return egl_surface;
}

static void
handle_special_event(EGLShimDisplay *dpy, EGLShimSurface *surf,
                     xcb_present_generic_event_t *ge)
{
  switch (ge->evtype)
  {
    case XCB_PRESENT_COMPLETE_NOTIFY:
    {
#ifdef DBG
      xcb_present_complete_notify_event_t *ce =
          (xcb_present_complete_notify_event_t*) ge;

      DEBUG("XCB_PRESENT_COMPLETE_NOTIFY %d %llu %llu\n", ce->serial,
             (unsigned long long)ce->msc, (unsigned long long)ce->ust);
#endif
      break;
    }

    case XCB_PRESENT_EVENT_IDLE_NOTIFY:
    {
      xcb_present_idle_notify_event_t *ie =
          (xcb_present_idle_notify_event_t *)ge;
      EGLPixmapBuffer *pb =
          egl_surface_pixmap_buffer_find_by_pixmap(surf, ie->pixmap);

      assert(pb);

      if (pb->busy)
      {
        gbm_surface_release_buffer(surf->gbm_surface, pb->bo);
        pb->busy = False;
        surf->lock_count--;
      }

      DEBUG("XCB_PRESENT_EVENT_IDLE_NOTIFY %d\n", ie->serial);

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
poll_special_events(EGLShimDisplay *dpy, EGLShimSurface *surf)
{
  xcb_generic_event_t *ge;

  while ((ge = xcb_poll_for_special_event(dpy->xcb_conn, surf->ev)) != NULL)
    handle_special_event(dpy, surf, (xcb_present_generic_event_t *)ge);
}

static void
wait_special_event(EGLShimDisplay *dpy, EGLShimSurface *surf)
{
  xcb_generic_event_t *ge;

  ge = xcb_wait_for_special_event(dpy->xcb_conn, surf->ev);
  handle_special_event(dpy, surf, (xcb_present_generic_event_t*)ge);

  poll_special_events(dpy, surf);
}

EGLAPI EGLBoolean EGLAPIENTRY
eglSwapBuffers(EGLDisplay egl_dpy, EGLSurface surface)
{
  EGLShimDisplay *dpy;
  EGLShimSurface *surf;
  struct gbm_bo *bo;
  EGLPixmapBuffer *pb;

  DEBUG("%s\n", __FUNCTION__);

  if (!_eglSwapBuffers(egl_dpy, surface))
    return EGL_FALSE;

  dpy = egl_shim_display_find(egl_dpy);
  assert(dpy);

  surf = egl_shim_display_find_surface(dpy, surface);
  assert(surf);

  bo = gbm_surface_lock_front_buffer(surf->gbm_surface);
  pb = gbm_bo_get_user_data(bo);
  surf->lock_count++;

  if (surf->first_pixmap_presented)
  {
    DEBUG("locked count %d\n", surf->lock_count);
    poll_special_events(dpy, surf);

    while (surf->lock_count > 1)
      wait_special_event(dpy, surf);
  }

  if (!pb)
    pb = egl_pixmap_buffer_create(dpy, surf, bo);

  assert(!pb->busy);
  DEBUG("locked %d\n", pb->serial);

  if (!surf->first_pixmap_presented)
  {
    surf->first_pixmap_presented = True;
    egl_pixmap_buffer_present(dpy, surf, pb);
    wait_special_event(dpy, surf);
  }

  pb->busy = True;
  egl_pixmap_buffer_present(dpy, surf, pb);

  return EGL_TRUE;
}
