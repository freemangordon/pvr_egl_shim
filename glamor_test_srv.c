#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include "glamor_test.h"

static struct
{
  int fd;
  drmModeModeInfo *mode;
  uint32_t crtc_id;
  uint32_t connector_id;
} drm;

static struct {
  struct gbm_device *dev;
  struct gbm_bo *bo;
} gbm;

struct drm_fb {
  struct gbm_bo *bo;
  uint32_t fb_id;
};

static uint32_t
find_crtc_for_encoder(const drmModeRes *resources,
                      const drmModeEncoder *encoder)
{
  int i;

  for (i = 0; i < resources->count_crtcs; i++)
  {
    /* possible_crtcs is a bitmask as described here:
     * https://dvdhrm.wordpress.com/2012/09/13/linux-drm-mode-setting-api
     */
    const uint32_t crtc_mask = 1 << i;
    const uint32_t crtc_id = resources->crtcs[i];

    if (encoder->possible_crtcs & crtc_mask)
      return crtc_id;
  }

  /* no match found */
  return -1;
}

static uint32_t
find_crtc_for_connector(const drmModeRes *resources,
                        const drmModeConnector *connector)
{
  int i;

  for (i = 0; i < connector->count_encoders; i++)
  {
    const uint32_t encoder_id = connector->encoders[i];

    drmModeEncoder *encoder = drmModeGetEncoder(drm.fd, encoder_id);

    if (encoder)
    {
      const uint32_t crtc_id = find_crtc_for_encoder(resources, encoder);

      drmModeFreeEncoder(encoder);

      if (crtc_id)
        return crtc_id;
    }
  }

  /* no match found */
  return -1;
}

static int
init_drm(void)
{
  drmModeRes *resources;
  drmModeConnector *connector = NULL;
  drmModeEncoder *encoder = NULL;
  int i, area;

  drm.fd = open("/dev/dri/card1", O_RDWR);

  if (drm.fd < 0)
  {
    printf("could not open drm device\n");
    return -1;
  }

  resources = drmModeGetResources(drm.fd);

  if (!resources)
  {
    printf("drmModeGetResources failed: %s\n", strerror(errno));
    return -1;
  }

  /* find a connected connector: */
  for (i = 0; i < resources->count_connectors; i++)
  {
    connector = drmModeGetConnector(drm.fd, resources->connectors[i]);

    if (connector->connection == DRM_MODE_CONNECTED)
    {
      /* it's connected, let's use this! */
      break;
    }

    drmModeFreeConnector(connector);
    connector = NULL;
  }

  if (!connector)
  {
    /* we could be fancy and listen for hotplug events and wait for
                 * a connector..
                 */
    printf("no connected connector!\n");
    return -1;
  }

  /* find prefered mode or the highest resolution mode: */
  for (i = 0, area = 0; i < connector->count_modes; i++)
  {
    int current_area;

    drmModeModeInfo *current_mode = &connector->modes[i];

    if (current_mode->type & DRM_MODE_TYPE_PREFERRED)
      drm.mode = current_mode;

    current_area = current_mode->hdisplay * current_mode->vdisplay;

    if (current_area > area)
    {
      drm.mode = current_mode;
      area = current_area;
    }
  }

  if (!drm.mode)
  {
    printf("could not find mode!\n");
    return -1;
  }

  /* find encoder: */
  for (i = 0; i < resources->count_encoders; i++)
  {
    encoder = drmModeGetEncoder(drm.fd, resources->encoders[i]);

    if (encoder->encoder_id == connector->encoder_id)
      break;

    drmModeFreeEncoder(encoder);
    encoder = NULL;
  }

  if (encoder)
  {
    drm.crtc_id = encoder->crtc_id;
  }
  else
  {
    uint32_t crtc_id = find_crtc_for_connector(resources, connector);

    if (crtc_id == 0)
    {
      printf("no crtc found!\n");
      return -1;
    }

    drm.crtc_id = crtc_id;
  }

  drm.connector_id = connector->connector_id;

  return 0;
}

static int
init_gbm()
{
  gbm.dev = gbm_create_device(drm.fd);

  gbm.bo = gbm_bo_create(
        gbm.dev, drm.mode->hdisplay, drm.mode->vdisplay, GBM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  if (!gbm.bo)
  {
    printf("failed to create gbm bo\n");
    return -1;
  }

  return 0;
}

static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
  struct drm_fb *fb = data;
  /*struct gbm_device *gbm = gbm_bo_get_device(bo);*/

  if (fb->fb_id)
    drmModeRmFB(drm.fd, fb->fb_id);

  free(fb);
}

static struct drm_fb *
drm_fb_get_from_bo(struct gbm_bo *bo)
{
  struct drm_fb *fb = gbm_bo_get_user_data(bo);
  uint32_t width, height, stride, handle;
  int ret;

  if (fb)
    return fb;

  fb = calloc(1, sizeof *fb);
  fb->bo = bo;

  width = gbm_bo_get_width(bo);
  height = gbm_bo_get_height(bo);
  stride = gbm_bo_get_stride(bo);
  handle = gbm_bo_get_handle(bo).u32;

  ret = drmModeAddFB(drm.fd, width, height, 24, 32, stride, handle, &fb->fb_id);

  if (ret)
  {
    printf("failed to create fb: %s\n", strerror(errno));
    free(fb);
    return NULL;
  }

  gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

  return fb;
}

int
main(int argc, char *argv[])
{
  int ret;
  struct drm_fb *fb;
  char buf[256];

  if ((ret = init_drm()))
  {
    printf("failed to initialize DRM\n");
    return ret;
  }

  if ((ret = init_gbm()))
  {
          printf("failed to initialize GBM\n");
          return ret;
  }

  fb = drm_fb_get_from_bo(gbm.bo);

  /* set mode: */
  ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0,
                  &drm.connector_id, 1, drm.mode);
  if (ret)
  {
          printf("failed to set mode: %s\n", strerror(errno));
          return ret;
  }

  printf("%d\n%d\n", drm.mode->hdisplay, drm.mode->vdisplay);
  fflush(stdout);

  while (fgets(buf, sizeof(buf), stdin))
  {
    struct msg *msg = (struct msg *)buf;

    switch (msg->cmd)
    {
      case CMD_PIXMAP_CREATE:
      {
        uint32_t id = hexstrntoul(msg->u.create.id);
        uint32_t fd = hexstrntoul(msg->u.create.fd);

        printf("created pixmap id %d fd %d\n", id, fd);
        fflush(stdout);

        break;
      }
      case CMD_PIXMAP_PRESENT:
      {
        uint32_t id = hexstrntoul(msg->u.create.id);

        printf("PRESENTED id %d\n", id);
        fflush(stdout);

        break;
      }
    }
  }

  return ret;
}
