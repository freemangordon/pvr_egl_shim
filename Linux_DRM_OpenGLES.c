// gcc -o drmgl Linux_DRM_OpenGLES.c `pkg-config --cflags --libs libdrm` -lgbm -lEGL -lGLESv2

/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 * Copyright (c) 2017 Miouyouyou <Myy> <myy@miouyouyou.fr>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

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


#include <assert.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static int SHOW_FPS = 1;

static struct {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	GLuint program;
	GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
	GLuint vbo;
	GLuint positionsoffset, colorsoffset, normalsoffset;
} gl;

static struct {
	struct gbm_device *dev;
	struct gbm_surface *surface;
} gbm;

static struct {
	int fd;
	drmModeModeInfo *mode;
	uint32_t crtc_id;
	uint32_t connector_id;
} drm;

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
};

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

static uint32_t find_crtc_for_encoder(const drmModeRes *resources,
				      const drmModeEncoder *encoder) {
	int i;

	for (i = 0; i < resources->count_crtcs; i++) {
		/* possible_crtcs is a bitmask as described here:
		 * https://dvdhrm.wordpress.com/2012/09/13/linux-drm-mode-setting-api
		 */
		const uint32_t crtc_mask = 1 << i;
		const uint32_t crtc_id = resources->crtcs[i];
		if (encoder->possible_crtcs & crtc_mask) {
			return crtc_id;
		}
	}

	/* no match found */
	return -1;
}

static uint32_t find_crtc_for_connector(const drmModeRes *resources,
					const drmModeConnector *connector) {
	int i;

	for (i = 0; i < connector->count_encoders; i++) {
		const uint32_t encoder_id = connector->encoders[i];
		drmModeEncoder *encoder = drmModeGetEncoder(drm.fd, encoder_id);

		if (encoder) {
			const uint32_t crtc_id = find_crtc_for_encoder(resources, encoder);

			drmModeFreeEncoder(encoder);
			if (crtc_id != 0) {
				return crtc_id;
			}
		}
	}

	/* no match found */
	return -1;
}

static int init_drm(void)
{
	drmModeRes *resources;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int i, area;

	drm.fd = open("/dev/dri/card1", O_RDWR);

	if (drm.fd < 0) {
		printf("could not open drm device\n");
		return -1;
	}

	resources = drmModeGetResources(drm.fd);
	if (!resources) {
		printf("drmModeGetResources failed: %s\n", strerror(errno));
		return -1;
	}

	/* find a connected connector: */
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm.fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* it's connected, let's use this! */
			break;
		}
		drmModeFreeConnector(connector);
		connector = NULL;
	}

	if (!connector) {
		/* we could be fancy and listen for hotplug events and wait for
		 * a connector..
		 */
		printf("no connected connector!\n");
		return -1;
	}

	/* find prefered mode or the highest resolution mode: */
	for (i = 0, area = 0; i < connector->count_modes; i++) {
		drmModeModeInfo *current_mode = &connector->modes[i];

		if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
			drm.mode = current_mode;
		}

		int current_area = current_mode->hdisplay * current_mode->vdisplay;
		if (current_area > area) {
			drm.mode = current_mode;
			area = current_area;
		}
	}

	if (!drm.mode) {
		printf("could not find mode!\n");
		return -1;
	}

	/* find encoder: */
	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(drm.fd, resources->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id)
			break;
		drmModeFreeEncoder(encoder);
		encoder = NULL;
	}

	if (encoder) {
		drm.crtc_id = encoder->crtc_id;
	} else {
		uint32_t crtc_id = find_crtc_for_connector(resources, connector);
		if (crtc_id == 0) {
			printf("no crtc found!\n");
			return -1;
		}

		drm.crtc_id = crtc_id;
	}

	drm.connector_id = connector->connector_id;

	return 0;
}

static int init_gbm(void)
{
	gbm.dev = gbm_create_device(drm.fd);

	gbm.surface = gbm_surface_create(gbm.dev,
			drm.mode->hdisplay, drm.mode->vdisplay,
			GBM_FORMAT_XRGB8888,
			GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!gbm.surface) {
		printf("failed to create gbm surface\n");
		return -1;
	}

	return 0;
}

GLuint tex;
GLuint fbo;

PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC _glEGLImageTargetTexture2DOES;

EGLImageKHR
createImage(struct gbm_bo *bo)
{
  EGLImageKHR image;

  image = eglCreateImageKHR(gl.display,
                            EGL_NO_CONTEXT,
                            EGL_NATIVE_PIXMAP_KHR, bo, NULL);

  return image;
}

GLuint
CreateSimpleImageTexture(EGLImageKHR image)
{
  GLuint textureId;

  glGenTextures(1, &textureId);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  _glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);

  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

  return textureId;
}

GLuint
CreateSimpleTexture2D()
{
  struct gbm_bo *bo = gbm_bo_create(gbm.dev, 300, 300, GBM_FORMAT_XRGB8888,
                                    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  GLuint texture;

  EGLImageKHR image = createImage(bo);

  texture = CreateSimpleImageTexture(image);

  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE)
      printf("Could not validate framebuffer\n");

  glClearColor(1.0, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return texture;
}

GLuint programObject;
GLint positionLoc;
GLint texCoordLoc;
GLint samplerLoc;

static int init_gl(void)
{
	EGLint major, minor, n;

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const EGLint config_attribs[] = {
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

	eglCreateImageKHR =
	    (void *) eglGetProcAddress("eglCreateImageKHR");

	assert(eglCreateImageKHR != NULL);

	_glEGLImageTargetTexture2DOES =
	    (void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");

	assert(_glEGLImageTargetTexture2DOES != NULL);

	gl.display = get_platform_display(EGL_PLATFORM_GBM_KHR, gbm.dev, NULL);

	if (!eglInitialize(gl.display, &major, &minor)) {
		printf("failed to initialize\n");
		return -1;
	}

	printf("Using display %p with EGL version %d.%d\n",
			gl.display, major, minor);

	printf("EGL Version \"%s\"\n", eglQueryString(gl.display, EGL_VERSION));
	printf("EGL Vendor \"%s\"\n", eglQueryString(gl.display, EGL_VENDOR));
	printf("EGL Extensions \"%s\"\n", eglQueryString(gl.display, EGL_EXTENSIONS));

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	if (!eglChooseConfig(gl.display, config_attribs, &gl.config, 1, &n) || n != 1) {
		printf("failed to choose config: %d\n", n);
		return -1;
	}

	gl.context = eglCreateContext(gl.display, gl.config,
			EGL_NO_CONTEXT, context_attribs);
	if (gl.context == NULL) {
		printf("failed to create context\n");
		return -1;
	}

	gl.surface = eglCreateWindowSurface(gl.display, gl.config, gbm.surface, NULL);
	if (gl.surface == EGL_NO_SURFACE) {
		printf("failed to create egl surface\n");
		return -1;
	}

	/* connect the context to the surface */
	eglMakeCurrent(gl.display, gl.surface, gl.surface, gl.context);

	tex = CreateSimpleTexture2D();

	printf("GL Extensions: \"%s\"\n", glGetString(GL_EXTENSIONS));

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

/* Draw code here */
static void draw(uint32_t i)
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

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glClearColor(0.3f, (i%100) * .01, 0.5f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glClearColor(((i/2)%100) * .01, 0.3f, 0.5f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Use the program object
  glUseProgram(programObject);

  // Load the vertex position
  glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vVertices);
  // Load the texture coordinate
  glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3]);

  glEnableVertexAttribArray(positionLoc);
  glEnableVertexAttribArray(texCoordLoc);

  // Bind the texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);

  // Set the sampler texture unit to 0
  glUniform1i(samplerLoc, 0);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
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

static struct drm_fb * drm_fb_get_from_bo(struct gbm_bo *bo)
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
	if (ret) {
		printf("failed to create fb: %s\n", strerror(errno));
		free(fb);
		return NULL;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}

static void page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	int *waiting_for_flip = data;
	*waiting_for_flip = 0;
}

static uint32_t TimeElapsedMs(const struct timespec *tStartTime,
			      const struct timespec *tEndTime)
{
	return 1000 * (tEndTime->tv_sec - tStartTime->tv_sec) +
		(tEndTime->tv_nsec - tStartTime->tv_nsec) / 1000000;
}

static void CalculateFrameRate()
{
	static float framesPerSecond    = 0.0f;       // This will store our fps
	static struct timespec lastTime;
	static struct timespec currentTime;
	static int firstTime = 1;

	clock_gettime(CLOCK_MONOTONIC, &currentTime);
	if (firstTime) {
		lastTime = currentTime;
		firstTime = 0;
	}

	framesPerSecond++;

	if (TimeElapsedMs(&lastTime, &currentTime) > 5000)
	{
		lastTime = currentTime;

		if (SHOW_FPS == 1)  {
			fprintf(stderr, "\nCurrent Frames Per Second: %d\n\n",
				(int)(framesPerSecond / 5.0f));
		}

		framesPerSecond = 0;
	}
}

int main(int argc, char *argv[])
{
	fd_set fds;
	drmEventContext evctx = {
			.version = DRM_EVENT_CONTEXT_VERSION,
			.page_flip_handler = page_flip_handler,
	};
	struct gbm_bo *bo;
	struct drm_fb *fb;
	uint32_t i = 0;
	int ret;

	ret = init_drm();
	if (ret) {
		printf("failed to initialize DRM\n");
		return ret;
	}

	FD_ZERO(&fds);
	FD_SET(0, &fds);
	FD_SET(drm.fd, &fds);

	ret = init_gbm();
	if (ret) {
		printf("failed to initialize GBM\n");
		return ret;
	}

	ret = init_gl();
	if (ret) {
		printf("failed to initialize EGL\n");
		return ret;
	}

	/* clear the color buffer */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(gl.display, gl.surface);
	bo = gbm_surface_lock_front_buffer(gbm.surface);
	fb = drm_fb_get_from_bo(bo);

	/* set mode: */
	ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0,
			&drm.connector_id, 1, drm.mode);
	if (ret) {
		printf("failed to set mode: %s\n", strerror(errno));
		return ret;
	}

	while (1) {
		struct gbm_bo *next_bo;
		int waiting_for_flip = 1;

		draw(i++);

		eglSwapBuffers(gl.display, gl.surface);
		next_bo = gbm_surface_lock_front_buffer(gbm.surface);
		fb = drm_fb_get_from_bo(next_bo);

		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */

		ret = drmModePageFlip(drm.fd, drm.crtc_id, fb->fb_id,
				DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
		if (ret) {
			printf("failed to queue page flip: %s\n", strerror(errno));
			return -1;
		}

		while (waiting_for_flip) {
			ret = select(drm.fd + 1, &fds, NULL, NULL, NULL);
			if (ret < 0) {
				printf("select err: %s\n", strerror(errno));
				return ret;
			} else if (ret == 0) {
				printf("select timeout!\n");
				return -1;
			} else if (FD_ISSET(0, &fds)) {
				printf("user interrupted!\n");
				break;
			}
			drmHandleEvent(drm.fd, &evctx);
		}

		/* release last buffer to render on again: */
		gbm_surface_release_buffer(gbm.surface, bo);
		bo = next_bo;
		CalculateFrameRate();
	}

	return ret;
}
