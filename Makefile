CC=gcc
SHIM_PACKAGES := x11-xcb xcb-dri3 xcb-present gbm
SHIM_CFLAGS := $(shell pkg-config --cflags $(SHIM_PACKAGES)) -Wall -Werror -g \
	       -fPIC
SHIM_LIBS := $(shell pkg-config --libs $(SHIM_PACKAGES))
SHIM_OBJS = list.o xcb_dri3.o xcb_event.o egl_shim_display.o \
	    egl_shim_surface.o egl_pixmap.o egl_shim.o

DRM_OPENGLES2_PACKAGES = libdrm glesv2 gbm egl
DRM_OPENGLES2_CFLAGS := $(shell pkg-config --cflags $(DRM_OPENGLES2_PACKAGES)) \
			-Wall -Werror -g -fPIC
DRM_OPENGLES2_LIBS := $(shell pkg-config --libs $(DRM_OPENGLES2_PACKAGES))
DRM_OPENGLES2_OBJS = Linux_DRM_OpenGLES.o

DEPS = list.h xcb_dri3.h xcb_event.h egl_shim_display.h \
	egl_shim_surface.h egl_pixmap.h defs.h

SHIM_TARGET = egl_shim.so
DRM_OPENGLES2_TARGET = drm_opengles2

$(SHIM_OBJS): OBJS_CFLAGS=$(SHIM_CFLAGS) $(CFLAGS)
$(DRM_OPENGLES2_OBJS): OBJS_CFLAGS=$(DRM_OPENGLES2_CFLAGS) $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(OBJS_CFLAGS)

all: $(SHIM_TARGET) $(DRM_OPENGLES2_TARGET)

$(SHIM_TARGET): $(SHIM_OBJS)
	$(CC) -Wl,--no-undefined -shared -fPIC $(SHIM_LIBS)  -ldl $^ -o $@

$(DRM_OPENGLES2_TARGET) : $(DRM_OPENGLES2_OBJS)
	$(CC) -Wl,--no-undefined -fPIC $(DRM_OPENGLES2_LIBS)  -ldl $^ -o $@

clean:
	-rm -f $(SHIM_OBJS) $(SHIM_TARGET) $(DRM_OPENGLES2_TARGET) \
	       $(DRM_OPENGLES2_OBJS)
