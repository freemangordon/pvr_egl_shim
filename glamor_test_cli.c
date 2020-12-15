/*
 * glamor_test_cli.c
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include <gbm.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/egl.h>
#include <GLES/glext.h>

#include "glamor_test.h"

static struct {
  struct gbm_device *dev;
  struct gbm_surface *surface;
} gbm;

static struct {
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;
  EGLSurface surface;
} gl;

static uint32_t hdisplay;
static uint32_t vdisplay;
static int drm_fd = -1;
static int SHOW_FPS = 1;

static int
init_drm(void)
{
  drm_fd = open("/dev/dri/card1", O_RDWR);

  if (drm_fd < 0)
  {
    fprintf(stderr, "could not open drm device\n");
    return -1;
  }

  return 0;
}

static int
init_gbm()
{
  gbm.dev = gbm_create_device(drm_fd);

  gbm.surface = gbm_surface_create(
        gbm.dev, hdisplay, vdisplay, GBM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  if (!gbm.surface)
  {
    fprintf(stderr, "failed to create gbm bo\n");
    return -1;
  }

  return 0;
}

static int init_gl(void)
{
  EGLint major, minor, n;

  static const EGLint context_attribs[] =
  {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  static const EGLint config_attribs[] =
  {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_ALPHA_SIZE, 0,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
  get_platform_display =
      (void *) eglGetProcAddress("eglGetPlatformDisplayEXT");
  assert(get_platform_display != NULL);

  gl.display = get_platform_display(EGL_PLATFORM_GBM_KHR, gbm.dev, NULL);

  if (!eglInitialize(gl.display, &major, &minor))
  {
    fprintf(stderr, "failed to initialize\n");
    return -1;
  }

  if (!eglBindAPI(EGL_OPENGL_ES_API))
  {
    fprintf(stderr, "failed to bind api EGL_OPENGL_ES_API\n");
    return -1;
  }

  if (!eglChooseConfig(gl.display, config_attribs, &gl.config, 1, &n) || n != 1)
  {
    fprintf(stderr, "failed to choose config: %d\n", n);
    return -1;
  }

  gl.context = eglCreateContext(gl.display, gl.config,
                                EGL_NO_CONTEXT, context_attribs);
  if (gl.context == NULL)
  {
    fprintf(stderr, "failed to create context\n");
    return -1;
  }

  gl.surface = eglCreateWindowSurface(gl.display, gl.config, gbm.surface, NULL);

  if (gl.surface == EGL_NO_SURFACE)
  {
    fprintf(stderr, "failed to create egl surface\n");
    return -1;
  }

  /* connect the context to the surface */
  eglMakeCurrent(gl.display, gl.surface, gl.surface, gl.context);

  return 0;
}

static void
draw(uint32_t i)
{
  glClearColor(0, (i % 100) * .01, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

static void
destroy_data(struct gbm_bo *bo, void *data)
{
  free(data);
}

uint32_t *
pixmap_create(struct gbm_bo *bo, uint32_t pixmap_id)
{
  int fd = gbm_bo_get_fd(bo);
  uint32_t *id = malloc(sizeof(*id));
  char buf[256];

  *id = pixmap_id;
  gbm_bo_set_user_data(bo, id, destroy_data);

  printf("%c%08x%08x\n", CMD_PIXMAP_CREATE, pixmap_id, fd);
  fflush(stdout);
  close(fd);

  fgets(buf, sizeof(buf), stdin);
  fprintf(stderr, buf);

  return id;
}

int
pixmap_present(uint32_t id)
{
  printf("%c%08x\n", CMD_PIXMAP_PRESENT, id);
  fflush(stdout);

  return 0;
}

static uint32_t
TimeElapsedMs(const struct timespec *tStartTime,
              const struct timespec *tEndTime)
{
  return 1000 * (tEndTime->tv_sec - tStartTime->tv_sec) +
      (tEndTime->tv_nsec - tStartTime->tv_nsec) / 1000000;
}

static void
CalculateFrameRate()
{
  static float framesPerSecond    = 0.0f;       // This will store our fps
  static struct timespec lastTime;
  static struct timespec currentTime;
  static int firstTime = 1;

  clock_gettime(CLOCK_MONOTONIC, &currentTime);

  if (firstTime)
  {
    lastTime = currentTime;
    firstTime = 0;
  }

  framesPerSecond++;

  if (TimeElapsedMs(&lastTime, &currentTime) > 1000)
  {
    lastTime = currentTime;

    if (SHOW_FPS == 1)
    {
      fprintf(stderr, "\nCurrent Frames Per Second: %d\n\n",
              (int)(framesPerSecond / 1.0f));
    }

    framesPerSecond = 0;
  }
}

int
main(int argc, char *argv[])
{
  char buf[256];
  int ret;
  int i = 0;
  uint32_t *id;
  uint32_t pixmap_id = 0;

  struct gbm_bo *bo = NULL;
  struct gbm_bo *next_bo = NULL;

  scanf("%u", &hdisplay);
  scanf("%u", &vdisplay);

  if ((ret = init_drm()))
  {
    fprintf(stderr, "failed to initialize DRM\n");
    return ret;
  }

  fprintf(stderr, "Creating surface of size %dx%d\n", hdisplay, vdisplay);

  if ((ret = init_gbm()))
  {
    fprintf(stderr, "failed to initialize GBM\n");
    return ret;
  }

  if ((ret = init_gl()))
  {
    fprintf(stderr, "failed to initialize EGL\n");
    return ret;
  }

  while(1)
  {
    draw(i++);
    eglSwapBuffers(gl.display, gl.surface);
    next_bo = gbm_surface_lock_front_buffer(gbm.surface);

    id = gbm_bo_get_user_data(next_bo);

    if (!id)
      id = pixmap_create(next_bo, pixmap_id++);

    pixmap_present(*id);
    fgets(buf, sizeof(buf), stdin);
    fprintf(stderr, buf);

    if (bo)
      gbm_surface_release_buffer(gbm.surface, bo);

    bo = next_bo;

    CalculateFrameRate();
  }

  return ret;
}
