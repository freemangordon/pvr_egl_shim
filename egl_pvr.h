typedef struct _egl_config _EGLConfig;
typedef struct __DRIconfigRec __DRIconfig;
typedef struct _egl_display _EGLDisplay;
typedef pthread_mutex_t mtx_t;
typedef enum _egl_platform_type _EGLPlatformType;
typedef struct _egl_extensions _EGLExtensions;
typedef struct _egl_device _EGLDevice;
typedef struct _egl_driver _EGLDriver;
typedef struct _egl_array _EGLArray;
typedef struct _egl_resource _EGLResource;
typedef void *EGLLabelKHR;
typedef void (*EGLSetBlobFuncANDROID)(const void *, EGLsizeiANDROID, const void *, EGLsizeiANDROID);

struct _egl_extensions
{
  EGLBoolean ANDROID_blob_cache;
  EGLBoolean ANDROID_framebuffer_target;
  EGLBoolean ANDROID_image_native_buffer;
  EGLBoolean ANDROID_native_fence_sync;
  EGLBoolean ANDROID_recordable;
  EGLBoolean CHROMIUM_sync_control;
  EGLBoolean EXT_buffer_age;
  EGLBoolean EXT_create_context_robustness;
  EGLBoolean EXT_image_dma_buf_import;
  EGLBoolean EXT_image_dma_buf_import_modifiers;
  EGLBoolean EXT_pixel_format_float;
  EGLBoolean EXT_surface_CTA861_3_metadata;
  EGLBoolean EXT_surface_SMPTE2086_metadata;
  EGLBoolean EXT_swap_buffers_with_damage;
  unsigned int IMG_context_priority;
  EGLBoolean IMG_cl_image;
  EGLBoolean KHR_cl_event2;
  EGLBoolean KHR_config_attribs;
  EGLBoolean KHR_context_flush_control;
  EGLBoolean KHR_create_context;
  EGLBoolean KHR_create_context_no_error;
  EGLBoolean KHR_fence_sync;
  EGLBoolean KHR_get_all_proc_addresses;
  EGLBoolean KHR_gl_colorspace;
  EGLBoolean KHR_gl_renderbuffer_image;
  EGLBoolean KHR_gl_texture_2D_image;
  EGLBoolean KHR_gl_texture_3D_image;
  EGLBoolean KHR_gl_texture_cubemap_image;
  EGLBoolean KHR_image;
  EGLBoolean KHR_image_base;
  EGLBoolean KHR_image_pixmap;
  EGLBoolean KHR_mutable_render_buffer;
  EGLBoolean KHR_no_config_context;
  EGLBoolean KHR_partial_update;
  EGLBoolean KHR_reusable_sync;
  EGLBoolean KHR_surfaceless_context;
  EGLBoolean KHR_wait_sync;
  EGLBoolean MESA_drm_image;
  EGLBoolean MESA_image_dma_buf_export;
  EGLBoolean MESA_query_driver;
  EGLBoolean NOK_swap_region;
  EGLBoolean NOK_texture_from_pixmap;
  EGLBoolean NV_post_sub_buffer;
  EGLBoolean WL_bind_wayland_display;
  EGLBoolean WL_create_wayland_buffer_from_image;
};

enum _egl_platform_type
{
  _EGL_PLATFORM_X11 = 0x0,
  _EGL_PLATFORM_WAYLAND = 0x1,
  _EGL_PLATFORM_DRM = 0x2,
  _EGL_PLATFORM_ANDROID = 0x3,
  _EGL_PLATFORM_HAIKU = 0x4,
  _EGL_PLATFORM_SURFACELESS = 0x5,
  _EGL_NUM_PLATFORMS = 0x6,
  _EGL_INVALID_PLATFORM = 0xFFFFFFFF,
};

struct _egl_display
{
  _EGLDisplay *Next;
  mtx_t Mutex;
  _EGLPlatformType Platform;
  void *PlatformDisplay;
  _EGLDevice *Device;
  _EGLDriver *Driver;
  EGLBoolean Initialized;
  struct
  {
    EGLBoolean ForceSoftware;
    void *Platform;
  } Options;
  void *DriverData;
  EGLint Version;
  EGLint ClientAPIs;
  _EGLExtensions Extensions;
  uint8_t VersionString[100];
  uint8_t ClientAPIsString[100];
  uint8_t ExtensionsString[1000];
  _EGLArray *Configs;
  _EGLResource *ResourceLists[4];
  EGLLabelKHR Label;
  EGLSetBlobFuncANDROID BlobCacheSet;
  EGLGetBlobFuncANDROID BlobCacheGet;
};

struct _egl_config
{
  _EGLDisplay *Display;
  EGLint BufferSize;
  EGLint AlphaSize;
  EGLint BlueSize;
  EGLint GreenSize;
  EGLint RedSize;
  EGLint DepthSize;
  EGLint StencilSize;
  EGLint ConfigCaveat;
  EGLint ConfigID;
  EGLint Level;
  EGLint MaxPbufferHeight;
  EGLint MaxPbufferPixels;
  EGLint MaxPbufferWidth;
  EGLint NativeRenderable;
  EGLint NativeVisualID;
  EGLint NativeVisualType;
  EGLint Samples;
  EGLint SampleBuffers;
  EGLint SurfaceType;
  EGLint TransparentType;
  EGLint TransparentBlueValue;
  EGLint TransparentGreenValue;
  EGLint TransparentRedValue;
  EGLint BindToTextureRGB;
  EGLint BindToTextureRGBA;
  EGLint MinSwapInterval;
  EGLint MaxSwapInterval;
  EGLint LuminanceSize;
  EGLint AlphaMaskSize;
  EGLint ColorBufferType;
  EGLint RenderableType;
  EGLint MatchNativePixmap;
  EGLint Conformant;
  EGLint YInvertedNOK;
  EGLint FramebufferTargetAndroid;
  EGLint RecordableAndroid;
  EGLint ComponentType;
};

struct dri2_egl_config
{
  _EGLConfig base;
  const __DRIconfig *dri_config[2][2];
};
