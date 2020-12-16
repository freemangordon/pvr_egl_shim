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

GLAMOR_TEST_PACKAGES = libdrm glesv2 gbm egl
GLAMOR_TEST_CFLAGS := $(shell pkg-config --cflags $(GLAMOR_TEST_PACKAGES)) \
		      -Wall -g -fPIC
GLAMOR_TEST_LIBS := $(shell pkg-config --libs $(GLAMOR_TEST_PACKAGES))
GLAMOR_TEST_SRV_OBJS = glamor_test_srv.o glamor_send_fd.o
GLAMOR_TEST_CLI_OBJS = glamor_test_cli.o glamor_send_fd.o

DEPS = list.h xcb_dri3.h xcb_event.h egl_shim_display.h \
	egl_shim_surface.h egl_pixmap.h defs.h

SHIM_TARGET = egl_shim.so
DRM_OPENGLES2_TARGET = drm_opengles2
GLAMOR_TEST_SRV_TARGET = glamor_srv
GLAMOR_TEST_CLI_TARGET = glamor_cli

$(SHIM_OBJS): OBJS_CFLAGS=$(SHIM_CFLAGS) $(CFLAGS)
$(DRM_OPENGLES2_OBJS): OBJS_CFLAGS=$(DRM_OPENGLES2_CFLAGS) $(CFLAGS)
$(GLAMOR_TEST_SRV_OBJS): OBJS_CFLAGS=$(GLAMOR_TEST_CFLAGS) $(CFLAGS)
$(GLAMOR_TEST_CLI_OBJS): OBJS_CFLAGS=$(GLAMOR_TEST_CFLAGS) $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(OBJS_CFLAGS)

all: $(SHIM_TARGET) $(DRM_OPENGLES2_TARGET) $(GLAMOR_TEST_SRV_TARGET) \
	$(GLAMOR_TEST_CLI_TARGET)

$(SHIM_TARGET): $(SHIM_OBJS)
	$(CC) -Wl,--no-undefined -shared -fPIC $(SHIM_LIBS)  -ldl $^ -o $@

$(DRM_OPENGLES2_TARGET): $(DRM_OPENGLES2_OBJS)
	$(CC) -fPIC $(DRM_OPENGLES2_LIBS)  -ldl $^ -o $@

$(GLAMOR_TEST_SRV_TARGET): $(GLAMOR_TEST_SRV_OBJS)
	$(CC) -fPIC $(GLAMOR_TEST_LIBS)  -ldl $^ -o $@

$(GLAMOR_TEST_CLI_TARGET): $(GLAMOR_TEST_CLI_OBJS)
	$(CC) -fPIC $(GLAMOR_TEST_LIBS)  -ldl $^ -o $@

clean:
	-rm -f $(SHIM_OBJS) $(SHIM_TARGET) $(DRM_OPENGLES2_TARGET) \
	       $(DRM_OPENGLES2_OBJS) $(GLAMOR_TEST_SRV_OBJS) \
	       $(GLAMOR_TEST_CLI_OBJS)
