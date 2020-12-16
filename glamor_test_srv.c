#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/egl.h>
#include <GLES/glext.h>

#include "glamor_test.h"

static struct
{
  int fd;
  drmModeModeInfo *mode;
  uint32_t crtc_id;
  uint32_t connector_id;
} drm;

static struct
{
  struct gbm_device *dev;
  struct gbm_bo *bo;
} gbm;

struct drm_fb {
  struct gbm_bo *bo;
  uint32_t fb_id;
};

static struct
{
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;
  GLuint program;
  GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
  GLuint vbo;
  GLuint positionsoffset, colorsoffset, normalsoffset;
} gl;

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

  fcntl(drm.fd, F_SETFD, fcntl(drm.fd, F_GETFD) | FD_CLOEXEC);
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

PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC _glEGLImageTargetTexture2DOES;
void GL_APIENTRY (*_glDiscardFramebufferEXT)(GLenum target, GLsizei numAttachments, const GLenum *attachments);

GLuint programObject;
GLint positionLoc;
GLint texCoordLoc;
GLint samplerLoc;

GLuint LoadShader(const char *shaderSrc, GLenum type)
{
  GLuint shader;
  GLint compiled;

  shader = glCreateShader(type);

  if(shader == 0)
    return 0;

  glShaderSource(shader, 1, &shaderSrc, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

  if(!compiled)
  {
    GLint infoLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

    if(infoLen > 1)
    {
      char* infoLog = malloc(sizeof(char) * infoLen);
      glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
      printf("Error compiling shader:\n%s\n", infoLog);
      free(infoLog);
    }

    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint
LoadProgram(const char *vertShaderSrc, const char *fragShaderSrc)
{
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint programObject;
    GLint linked;

    // Load the vertex/fragment shaders
    vertexShader = LoadShader(vertShaderSrc, GL_VERTEX_SHADER);
    if (vertexShader == 0)
        return 0;

    fragmentShader = LoadShader(fragShaderSrc, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return 0;
    }

    // Create the program object
    programObject = glCreateProgram();

    if (programObject == 0)
        return 0;

    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);

    // Link the program
    glLinkProgram(programObject);

    // Check the link status
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint infoLen = 0;

        glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen > 1)
        {
            char* infoLog = malloc(sizeof(char) * infoLen);

            glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
            printf("Error linking program:\n%s\n", infoLog);

            free(infoLog);
        }

        glDeleteProgram(programObject);
        return 0;
    }

    // Free up no longer needed shader resources
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return programObject;
}

static int
init_gl(void)
{
  EGLint major, minor, n;

  static const EGLint context_attribs[] =
  {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  static const EGLint config_attribs[] =
  {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display = NULL;
  get_platform_display =
      (void *) eglGetProcAddress("eglGetPlatformDisplayEXT");
  assert(get_platform_display != NULL);

  eglCreateImageKHR =
      (void *) eglGetProcAddress("eglCreateImageKHR");

  assert(eglCreateImageKHR != NULL);

  _glEGLImageTargetTexture2DOES =
      (void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");

  assert(_glEGLImageTargetTexture2DOES != NULL);

  _glDiscardFramebufferEXT =
            (void *) eglGetProcAddress("glDiscardFramebufferEXT");

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

  /* connect the context to the surface */
  eglMakeCurrent(gl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, gl.context);

  GLchar vShaderStr[] =
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying vec2 v_texCoord;     \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position; \n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n";

  GLchar fShaderStr[] =
      "#extension GL_OES_EGL_image_external : require      \n"
      "precision mediump float;                            \n"
      "varying vec2 v_texCoord;                            \n"
      "uniform samplerExternalOES s_texture;               \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  gl_FragColor = texture2D( s_texture, v_texCoord );\n"
      "}                                                   \n";

  programObject = LoadProgram(vShaderStr, fShaderStr);

  positionLoc = glGetAttribLocation(programObject, "a_position");
  texCoordLoc = glGetAttribLocation(programObject, "a_texCoord");

  // Get the sampler location
  samplerLoc = glGetUniformLocation(programObject, "s_texture");

  return 0;
}

EGLImageKHR
image_from_bo(struct gbm_bo *bo)
{
  EGLImageKHR image;

  image = eglCreateImageKHR(gl.display, EGL_NO_CONTEXT,
                            EGL_NATIVE_PIXMAP_KHR, bo, NULL);

  return image;
}

GLuint
texture_from_image(EGLImageKHR image)
{
  GLuint textureId;

  glGenTextures(1, &textureId);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  _glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);

  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

  return textureId;
}


GLuint bo_fbo = 0;
GLuint tex;

GLuint
texture_from_bo(struct gbm_bo *bo)
{
  return texture_from_image(image_from_bo(bo));
}


GLuint fbo_from_tex(GLuint tex)
{
  GLuint fbo;

  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, tex, 0);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE)
      printf("Could not validate framebuffer\n");

  return fbo;
}

static struct {
  GLuint tex;
}
pixmap[8] = {0};

static void
draw_pixmap(uint32_t id)
{
  GLushort indices[] =
      { 0, 1, 2, 0, 2, 3 };
  GLfloat vVertices[] = {    -0.95f,  0.95f, 0.0f,  // Position 0
                              0.0f,   0.0f,        // TexCoord 0
                             -0.95f, -0.95f, 0.0f,  // Position 1
                              0.0f,   1.0f,        // TexCoord 1
                              0.95f, -0.95f, 0.0f,  // Position 2
                              1.0f,   1.0f,        // TexCoord 2
                              0.95f,  0.95f, 0.0f,  // Position 3
                              1.0f,   0.0f         // TexCoord 3
  };

  glBindFramebuffer(GL_FRAMEBUFFER, bo_fbo);
  glViewport(0, 0, drm.mode->hdisplay, drm.mode->vdisplay);

  // Use the program object
  glUseProgram(programObject);

  // Load the vertex position
  glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE,
                        5 * sizeof(GLfloat), vVertices);
  // Load the texture coordinate
  glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE,
                        5 * sizeof(GLfloat), &vVertices[3]);

  glEnableVertexAttribArray(positionLoc);
  glEnableVertexAttribArray(texCoordLoc);

  // Bind the texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, pixmap[id].tex);

  // Set the sampler texture unit to 0
  glUniform1i(samplerLoc, 0);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

    static const GLenum attachments[] = {
        GL_DEPTH_ATTACHMENT,
        GL_STENCIL_ATTACHMENT
    };

  _glDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, attachments);
}

int
main(int argc, char *argv[])
{
  int ret;
  struct drm_fb *fb;
  char buf[256];

  unlink(SOCKET_NAME);

  int s = bind_named_socket(SOCKET_NAME);
  if (s == -1)
  {
    fprintf(stdout, "failed to bind to socket\n");
    return -1;
  }

  listen(s, 5);
  s = accept(s, 0, 0);

  fprintf(stderr, "server socket connected %d\n", s);

  if ((ret = init_drm()))
  {
    fprintf(stdout, "failed to initialize DRM\n");
    return ret;
  }

  if ((ret = init_gbm()))
  {
    fprintf(stdout, "failed to initialize GBM\n");
    return ret;
  }

  if ((ret = init_gl()))
  {
    fprintf(stdout, "failed to initialize EGL\n");
    return ret;
  }

  fb = drm_fb_get_from_bo(gbm.bo);

  tex = texture_from_bo(gbm.bo);
  bo_fbo = fbo_from_tex(tex);

  glClearColor(0, 1, 0, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  /* set mode: */
  ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0,
                       &drm.connector_id, 1, drm.mode);
  if (ret)
  {
          fprintf(stdout, "failed to set mode: %s\n", strerror(errno));
          return ret;
  }

  uint32_t tmp = drm.mode->hdisplay;
  write_fd(s, &tmp, sizeof(tmp), -1);
  tmp = drm.mode->vdisplay;
  write_fd(s, &tmp, sizeof(tmp), -1);

  int fd;

  while (read_fd(s, buf, sizeof(buf), &fd))
  {
    struct msg *msg = (struct msg *)buf;

    switch (msg->cmd)
    {
      case CMD_PIXMAP_CREATE:
      {
        uint32_t id = hexstrntoul(msg->u.create.id);
        struct gbm_bo *pixmap_bo;
        struct gbm_import_fd_data import_data =
        {
          .fd = fd,
          .width = hexstrntoul(msg->u.create.width),
          .height = hexstrntoul(msg->u.create.height),
          .stride = hexstrntoul(msg->u.create.stride),
          .format = hexstrntoul(msg->u.create.format)
        };

        fprintf(stderr, "id %d %d %d %d %d %x\n",
                id,
                import_data.fd,
                import_data.width,
                import_data.height,
                import_data.stride,
                import_data.format);
        pixmap_bo = gbm_bo_import(gbm.dev, GBM_BO_IMPORT_FD, &import_data, 0);

        close(import_data.fd);

        if (!pixmap_bo)
          return -1;

        pixmap[id].tex = texture_from_bo(pixmap_bo);
        gbm_bo_destroy(pixmap_bo);

        sprintf(buf, "created pixmap id %d fd %d\n", id, fd);
        write_fd(s, buf, strlen(buf) + 1, -1);

        break;
      }
      case CMD_PIXMAP_PRESENT:
      {
        uint32_t id = hexstrntoul(msg->u.create.id);

        draw_pixmap(id);

        sprintf(buf, "PRESENTED id %d\n", id);
        write_fd(s, buf, strlen(buf) + 1, -1);

        break;
      }
    }
  }

  return ret;
}
