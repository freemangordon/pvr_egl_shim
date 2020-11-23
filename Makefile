CC=gcc
PACKAGES = x11-xcb xcb-dri3 xcb-present gbm
CFLAGS += `pkg-config --cflags $(PACKAGES)` -Wall -Werror -g -fPIC
LIBS := `pkg-config --libs $(PACKAGES)`

SHIM_OBJS = list.o xcb_dri3.o xcb_event.o egl_shim_display.o \
	egl_shim_surface.o egl_pixmap.o egl_shim.o

DEPS = list.h xcb_dri3.h xcb_event.h egl_shim_display.h \
	egl_shim_surface.h egl_pixmap.h defs.h

SHIM_TARGET = egl_shim.so

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)


$(SHIM_TARGET): $(SHIM_OBJS)
	$(CC) -Wl,--no-undefined -shared -fPIC $(LIBS)  -ldl $^ -o $@

clean:
	-rm -f *.o $(SHIM_TARGET)
