/*
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "util/debug.h"
#include "util/macros.h"
#include "util/bitscan.h"

#include "egl_dri2.h"
#include "loader.h"

typedef enum {
    HAL_PIXEL_FORMAT_RGBA_8888 = 1,
    HAL_PIXEL_FORMAT_RGBX_8888 = 2,
    HAL_PIXEL_FORMAT_RGB_888 = 3,
    HAL_PIXEL_FORMAT_RGB_565 = 4,
    HAL_PIXEL_FORMAT_BGRA_8888 = 5,
    HAL_PIXEL_FORMAT_YCBCR_422_SP = 16,
    HAL_PIXEL_FORMAT_YCRCB_420_SP = 17,
    HAL_PIXEL_FORMAT_YCBCR_422_I = 20,
    HAL_PIXEL_FORMAT_RGBA_FP16 = 22,
    HAL_PIXEL_FORMAT_RAW16 = 32,
    HAL_PIXEL_FORMAT_BLOB = 33,
    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED = 34,
    HAL_PIXEL_FORMAT_YCBCR_420_888 = 35,
    HAL_PIXEL_FORMAT_RAW_OPAQUE = 36,
    HAL_PIXEL_FORMAT_RAW10 = 37,
    HAL_PIXEL_FORMAT_RAW12 = 38,
    HAL_PIXEL_FORMAT_RGBA_1010102 = 43,
    HAL_PIXEL_FORMAT_Y8 = 538982489,
    HAL_PIXEL_FORMAT_Y16 = 540422489,
    HAL_PIXEL_FORMAT_YV12 = 842094169,
} android_pixel_format_t;

static int
get_format_bpp(int native)
{
   int bpp;

   switch (native) {
   case HAL_PIXEL_FORMAT_RGBA_FP16:
      bpp = 8;
      break;
   case HAL_PIXEL_FORMAT_RGBA_8888:
   case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      /*
       * HACK: Hardcode this to RGBX_8888 as per cros_gralloc hack.
       * TODO: Remove this once https://issuetracker.google.com/32077885 is fixed.
       */
   case HAL_PIXEL_FORMAT_RGBX_8888:
   case HAL_PIXEL_FORMAT_BGRA_8888:
   case HAL_PIXEL_FORMAT_RGBA_1010102:
      bpp = 4;
      break;
   case HAL_PIXEL_FORMAT_RGB_565:
      bpp = 2;
      break;
   default:
      bpp = 0;
      break;
   }

   return bpp;
}

static void
swrastCreateDrawable(struct dri2_egl_display * dri2_dpy,
                     struct dri2_egl_surface * dri2_surf)
{
   ANativeWindow_lock(dri2_surf->window, &dri2_surf->buffer, NULL);
   dri2_surf->buffer_size = get_format_bpp(dri2_surf->buffer.format) * dri2_surf->buffer.stride * dri2_surf->buffer.height;   
   ANativeWindow_unlockAndPost(dri2_surf->window);
}

static void
swrastDestroyDrawable(struct dri2_egl_display * dri2_dpy,
                      struct dri2_egl_surface * dri2_surf)
{

}

static bool
droid_get_drawable_info(__DRIdrawable * draw,
                      int *x, int *y, int *w, int *h,
                      void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(dri2_surf->base.Resource.Display);

   *x = 0;
   *y = 0;
   *w = ANativeWindow_getWidth(dri2_surf->window);
   *h = ANativeWindow_getHeight(dri2_surf->window);
   return true;
}

static void
swrastGetDrawableInfo(__DRIdrawable * draw,
                      int *x, int *y, int *w, int *h,
                      void *loaderPrivate)
{
   *x = *y = *w = *h = 0;
   droid_get_drawable_info(draw, x, y, w, h, loaderPrivate);
}

static void
swrastPutImage(__DRIdrawable * draw, int op,
               int x, int y, int w, int h,
               char *data, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(dri2_surf->base.Resource.Display);

   ANativeWindow_lock(dri2_surf->window, &dri2_surf->buffer, NULL);
   memcpy(dri2_surf->buffer.bits, data, dri2_surf->buffer_size);
   ANativeWindow_unlockAndPost(dri2_surf->window);
}

static void
swrastGetImage(__DRIdrawable * read,
               int x, int y, int w, int h,
               char *data, void *loaderPrivate)
{
   /* FIXME: Does this necessary? */
#if 0
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(dri2_surf->base.Resource.Display);
   
   ANativeWindow_lock(dri2_surf->window, &dri2_surf->buffer, NULL);
   memcpy(data, dri2_surf->buffer.bits, dri2_surf->buffer_size);
   ANativeWindow_unlockAndPost(dri2_surf->window);
#endif
}

/**
 * Called via eglCreateWindowSurface(), drv->CreateWindowSurface().
 */
static _EGLSurface *
dri2_droid_create_surface(_EGLDisplay *disp, EGLint type, _EGLConfig *conf,
                        void *native_surface, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   ANativeWindow *window = native_surface;
   const __DRIconfig *config;

   dri2_surf = calloc(1, sizeof *dri2_surf);
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "dri2_create_surface");
      return NULL;
   }
   
   if (!dri2_init_surface(&dri2_surf->base, disp, type, conf, attrib_list,
                          false, native_surface))
      goto cleanup_surf;

   dri2_surf->drawable = native_surface;

   config = dri2_get_dri_config(dri2_conf, type,
                                dri2_surf->base.GLColorspace);

   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_surf;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_surf;

   if (type == EGL_WINDOW_BIT) {
      
      dri2_surf->base.Width = ANativeWindow_getWidth(window);
      dri2_surf->base.Height = ANativeWindow_getHeight(window);
      ANativeWindow_acquire(window);
      dri2_surf->window = window;
   }
   
   swrastCreateDrawable(dri2_dpy, dri2_surf);

   /* we always copy the back buffer to front */
   dri2_surf->base.PostSubBufferSupportedNV = EGL_TRUE;

   return &dri2_surf->base;

 cleanup_dri_drawable:
   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);
 cleanup_surf:
   free(dri2_surf);

   return NULL;
}

/**
 * Called via eglCreateWindowSurface(), drv->CreateWindowSurface().
 */
static _EGLSurface *
dri2_droid_create_window_surface(_EGLDisplay *disp, _EGLConfig *conf,
                               void *native_window, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   _EGLSurface *surf;

   surf = dri2_droid_create_surface(disp, EGL_WINDOW_BIT, conf,
                                  native_window, attrib_list);
   if (surf != NULL) {
      /* When we first create the DRI2 drawable, its swap interval on the
       * server side is 1.
       */
      surf->SwapInterval = 1;
   }

   return surf;
}

static _EGLSurface *
dri2_droid_create_pixmap_surface(_EGLDisplay *disp, _EGLConfig *conf,
                               void *native_pixmap, const EGLint *attrib_list)
{
   return dri2_droid_create_surface(disp, EGL_PIXMAP_BIT, conf,
                                  native_pixmap, attrib_list);
}

static _EGLSurface *
dri2_droid_create_pbuffer_surface(_EGLDisplay *disp, _EGLConfig *conf,
                                const EGLint *attrib_list)
{
   return dri2_droid_create_surface(disp, EGL_PBUFFER_BIT, conf,
                                  NULL, attrib_list);
}

static EGLBoolean
dri2_droid_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);
   
   swrastDestroyDrawable(dri2_dpy, dri2_surf);

   if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
      ANativeWindow_release(dri2_surf->window);
   }

   dri2_fini_surface(surf);
   free(surf);

   return EGL_TRUE;
}

/**
 * Function utilizes swrastGetDrawableInfo to get surface
 * geometry from x server and calls default query surface
 * implementation that returns the updated values.
 *
 * In case of errors we still return values that we currently
 * have.
 */
static EGLBoolean
dri2_query_surface(_EGLDisplay *disp, _EGLSurface *surf,
                   EGLint attribute, EGLint *value)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
   int x, y, w, h;

   __DRIdrawable *drawable = dri2_dpy->vtbl->get_dri_drawable(surf);

   switch (attribute) {
   case EGL_WIDTH:
   case EGL_HEIGHT:
      if (droid_get_drawable_info(drawable, &x, &y, &w, &h, dri2_surf)) {
         surf->Width = w;
         surf->Height = h;
      }
      break;
   default:
      break;
   }
   return _eglQuerySurface(disp, surf, attribute, value);
}

static EGLBoolean
dri2_droid_add_configs_for_visuals(struct dri2_egl_display *dri2_dpy,
                                 _EGLDisplay *disp)
{
   int config_count = 0;
   EGLint surface_type = EGL_WINDOW_BIT;
   
   static const struct {
      int format;
      int rgba_shifts[4];
      unsigned int rgba_sizes[4];
   } visuals[] = {
      { HAL_PIXEL_FORMAT_RGBA_8888, { 0, 8, 16, 24 }, { 8, 8, 8, 8 } },
      { HAL_PIXEL_FORMAT_RGBX_8888, { 0, 8, 16, -1 }, { 8, 8, 8, 0 } },
      { HAL_PIXEL_FORMAT_RGB_565,   { 11, 5, 0, -1 }, { 5, 6, 5, 0 } },
      /* This must be after HAL_PIXEL_FORMAT_RGBA_8888, we only keep BGRA
       * visual if it turns out RGBA visual is not available.
       */
      { HAL_PIXEL_FORMAT_BGRA_8888, { 16, 8, 0, 24 }, { 8, 8, 8, 8 } },
   };
   
   unsigned int format_count[ARRAY_SIZE(visuals)] = { 0 };

   /* The nesting of loops is significant here. Also significant is the order
    * of the HAL pixel formats. Many Android apps (such as Google's official
    * NDK GLES2 example app), and even portions the core framework code (such
    * as SystemServiceManager in Nougat), incorrectly choose their EGLConfig.
    * They neglect to match the EGLConfig's EGL_NATIVE_VISUAL_ID against the
    * window's native format, and instead choose the first EGLConfig whose
    * channel sizes match those of the native window format while ignoring the
    * channel *ordering*.
    *
    * We can detect such buggy clients in logcat when they call
    * eglCreateSurface, by detecting the mismatch between the EGLConfig's
    * format and the window's format.
    *
    * As a workaround, we generate EGLConfigs such that all EGLConfigs for HAL
    * pixel format i precede those for HAL pixel format i+1. In my
    * (chadversary) testing on Android Nougat, this was good enough to pacify
    * the buggy clients.
    */
   bool has_rgba = false;
   for (int i = 0; i < ARRAY_SIZE(visuals); i++) {
      /* Only enable BGRA configs when RGBA is not available. BGRA configs are
       * buggy on stock Android.
       */
      if (visuals[i].format == HAL_PIXEL_FORMAT_BGRA_8888 && has_rgba)
         continue;
      for (int j = 0; dri2_dpy->driver_configs[j]; j++) {
         const EGLint surface_type = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

         const EGLint config_attrs[] = {
           EGL_NATIVE_VISUAL_ID,   visuals[i].format,
           EGL_NATIVE_VISUAL_TYPE, visuals[i].format,
           EGL_NONE
         };

         struct dri2_egl_config *dri2_conf =
            dri2_add_config(disp, dri2_dpy->driver_configs[j],
                            config_count + 1, surface_type, config_attrs,
                            visuals[i].rgba_shifts, visuals[i].rgba_sizes);
         if (dri2_conf) {
            if (dri2_conf->base.ConfigID == config_count + 1)
               config_count++;
            format_count[i]++;
         }
         if (visuals[i].format == HAL_PIXEL_FORMAT_RGBA_8888 && format_count[i])
            has_rgba = true;
      }
   }

   for (int i = 0; i < ARRAY_SIZE(format_count); i++) {
      if (!format_count[i]) {
         _eglLog(_EGL_WARNING, "No DRI config supports native format 0x%x",
                 visuals[i].format);
      }
   }

   if (!config_count) {
      _eglLog(_EGL_WARNING, "DRI2: failed to create any config");
      return EGL_FALSE;
   }

   return EGL_TRUE;
}

static EGLBoolean
dri2_droid_swap_buffers(_EGLDisplay *disp, _EGLSurface *draw)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);

   if (!dri2_dpy->flush) {
      dri2_dpy->core->swapBuffers(dri2_surf->dri_drawable);
      return EGL_TRUE;
   }

   return EGL_TRUE;
}

static unsigned
dri2_droid_get_capability(void *loaderPrivate, enum dri_loader_cap cap)
{
   /* Note: loaderPrivate is _EGLDisplay* */
   switch (cap) {
   case DRI_LOADER_CAP_RGBA_ORDERING:
      return 1;
   default:
      return 0;
   }
}

static const struct dri2_egl_display_vtbl dri2_droid_swrast_display_vtbl = {
   .authenticate = NULL,
   .create_window_surface = dri2_droid_create_window_surface,
   .create_pixmap_surface = dri2_droid_create_pixmap_surface,
   .create_pbuffer_surface = dri2_droid_create_pbuffer_surface,
   .destroy_surface = dri2_droid_destroy_surface,
   .create_image = dri2_create_image_khr,
   .swap_buffers = dri2_droid_swap_buffers,
   /* XXX: should really implement this since X11 has pixmaps */
   .query_surface = dri2_query_surface,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
};

static const __DRIswrastLoaderExtension swrast_loader_extension = {
   .base = { __DRI_SWRAST_LOADER, 1 },

   .getDrawableInfo = swrastGetDrawableInfo,
   .putImage        = swrastPutImage,
   .getImage        = swrastGetImage,
};

static const __DRIimageLoaderExtension image_loader_extension = {
   .base = { __DRI_IMAGE_LOADER, 2 },

   .getBuffers               = NULL,
   .flushFrontBuffer         = NULL,
   .getCapability            = dri2_droid_get_capability,
   .flushSwapBuffers         = NULL,
   .destroyLoaderImageState  = NULL,
};

static const __DRIextension *swrast_loader_extensions[] = {
   &swrast_loader_extension.base,
   &image_loader_extension.base,
   &image_lookup_extension.base,
   NULL,
};

static EGLBoolean
dri2_get_swdroid_connection(_EGLDisplay *disp,
                        struct dri2_egl_display *dri2_dpy)
{
   disp->DriverData = (void *) dri2_dpy;
   dri2_dpy->own_device = true;
   
   return EGL_TRUE;
}

static EGLBoolean
dri2_initialize_droid_swrast(_EGLDisplay *disp)
{
   _EGLDevice *dev;
   struct dri2_egl_display *dri2_dpy;

   dri2_dpy = calloc(1, sizeof *dri2_dpy);
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_dpy->fd = -1;
   if (!dri2_get_swdroid_connection(disp, dri2_dpy))
      goto cleanup;

   dev = _eglAddDevice(dri2_dpy->fd, true);
   if (!dev) {
      _eglError(EGL_NOT_INITIALIZED, "DRI2: failed to find EGLDevice");
      goto cleanup;
   }

   disp->Device = dev;

   /*
    * Every hardware driver_name is set using strdup. Doing the same in
    * here will allow is to simply free the memory at dri2_terminate().
    */
   dri2_dpy->driver_name = strdup("swrast");
   if (!dri2_load_driver_swrast(disp))
      goto cleanup;

   dri2_dpy->loader_extensions = swrast_loader_extensions;

   if (!dri2_create_screen(disp))
      goto cleanup;

   if (!dri2_setup_extensions(disp))
      goto cleanup;

   dri2_setup_screen(disp);

   if (!dri2_droid_add_configs_for_visuals(dri2_dpy, disp))
      goto cleanup;

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &dri2_droid_swrast_display_vtbl;

   return EGL_TRUE;

 cleanup:
   dri2_display_destroy(disp);
   return EGL_FALSE;
}

EGLBoolean
dri2_initialize_swdroid(_EGLDisplay *disp)
{
   if (disp->Options.ForceSoftware)
      return dri2_initialize_droid_swrast(disp);

   return EGL_FALSE;
}
