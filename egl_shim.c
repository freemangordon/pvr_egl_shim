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

#include <gbm.h>

#include <X11/Xutil.h>

#include "egl_pvr.h"

static EGLAPI EGLDisplay EGLAPIENTRY (*_eglGetDisplay)(EGLNativeDisplayType display_id) = 0;
static EGLAPI EGLBoolean EGLAPIENTRY (*_eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
static EGLAPI EGLSurface EGLAPIENTRY (*_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) = 0;

static struct gbm_device *gbm = 0;

#define DRI_CARD "/dev/dri/card1"

static void 
init(void) 
{
  static int init_done = 0;
  
  if (init_done)
    return;
  
  _eglGetDisplay = dlsym(RTLD_NEXT, "eglGetDisplay");
  _eglGetConfigAttrib = dlsym(RTLD_NEXT, "eglGetConfigAttrib");
  _eglCreateWindowSurface = dlsym(RTLD_NEXT, "eglCreateWindowSurface");
  
  int fd = open(DRI_CARD, O_RDWR | FD_CLOEXEC);
  
  if (fd < 0) 
  {
    printf("cannot open %s - %s\n", DRI_CARD, strerror(errno));
    return;
  }
  
  gbm = gbm_create_device(fd);
  
  if (!gbm) 
  {
    printf("cannot creategbm device\n");
    return;
  }
  
  init_done = 1;
}

EGLAPI EGLDisplay EGLAPIENTRY 
eglGetDisplay(EGLNativeDisplayType display_id)
{
  init();
  printf("_eglGetDisplay\n");
  return _eglGetDisplay((EGLNativeDisplayType)gbm);
}

EGLAPI EGLBoolean EGLAPIENTRY 
eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, 
                   EGLint *value)
{
  Display *x_dpy;
  //_EGLDisplay *_dpy = (_EGLDisplay *)dpy;
  XVisualInfo vinfo;
  
  if (attribute != EGL_NATIVE_VISUAL_ID)
    return _eglGetConfigAttrib(dpy, config, attribute, value);
  
  init();
  
  x_dpy = XOpenDisplay(NULL);

  if (!XMatchVisualInfo(x_dpy,  DefaultScreen(x_dpy), 32, TrueColor, &vinfo))
    return EGL_FALSE;
  
  *value = vinfo.visualid;
  
  return EGL_TRUE;
}

EGLAPI EGLSurface EGLAPIENTRY 
eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, 
                       EGLNativeWindowType win, const EGLint *attrib_list)
{
  struct dri2_egl_config *c = (struct dri2_egl_config *)config;
  
  win = (EGLNativeWindowType)gbm_surface_create(gbm, 540, 960, c->base.NativeVisualID, 
                           GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  
  return _eglCreateWindowSurface(dpy, config, win, attrib_list);
}
