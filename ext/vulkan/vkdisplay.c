/*
 * GStreamer
 * Copyright (C) 2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
 * Copyright (C) 2013 Matthew Waters <ystreet00@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vkdisplay.h"

#if GST_VULKAN_HAVE_WINDOW_X11
#include "x11/vkdisplay_x11.h"
#endif
#if GST_VULKAN_HAVE_WINDOW_XCB
#include "xcb/vkdisplay_xcb.h"
#endif

GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);
#define GST_CAT_DEFAULT gst_vulkan_display_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static void
_init_debug (void)
{
  static volatile gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkandisplay", 0,
        "Vulkan display");
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
    g_once_init_leave (&_init, 1);
  }
}

G_DEFINE_TYPE_WITH_CODE (GstVulkanDisplay, gst_vulkan_display, GST_TYPE_OBJECT,
    _init_debug ());

#define GST_VULKAN_DISPLAY_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE((o), GST_TYPE_VULKAN_DISPLAY, GstVulkanDisplayPrivate))

enum
{
  SIGNAL_0,
  CREATE_CONTEXT,
  LAST_SIGNAL
};

/* static guint gst_vulkan_display_signals[LAST_SIGNAL] = { 0 }; */

static void gst_vulkan_display_finalize (GObject * object);
static gpointer gst_vulkan_display_default_get_handle (GstVulkanDisplay *
    display);
static GstVulkanWindow
    * gst_vulkan_display_default_create_window (GstVulkanDisplay * display);

struct _GstVulkanDisplayPrivate
{
  GThread *event_thread;

  GMutex thread_lock;
  GCond thread_cond;
};

static gpointer
_event_thread_main (GstVulkanDisplay * display)
{
  g_mutex_lock (&display->priv->thread_lock);

  display->main_context = g_main_context_new ();
  display->main_loop = g_main_loop_new (display->main_context, FALSE);

  g_cond_broadcast (&display->priv->thread_cond);
  g_mutex_unlock (&display->priv->thread_lock);

  g_main_loop_run (display->main_loop);

  g_mutex_lock (&display->priv->thread_lock);

  g_main_loop_unref (display->main_loop);
  g_main_context_unref (display->main_context);

  display->main_loop = NULL;
  display->main_context = NULL;

  g_cond_broadcast (&display->priv->thread_cond);
  g_mutex_unlock (&display->priv->thread_lock);

  return NULL;
}

static void
gst_vulkan_display_class_init (GstVulkanDisplayClass * klass)
{
  g_type_class_add_private (klass, sizeof (GstVulkanDisplayPrivate));

  klass->get_handle = gst_vulkan_display_default_get_handle;
  klass->create_window = gst_vulkan_display_default_create_window;

  G_OBJECT_CLASS (klass)->finalize = gst_vulkan_display_finalize;
}

static void
gst_vulkan_display_init (GstVulkanDisplay * display)
{
  display->priv = GST_VULKAN_DISPLAY_GET_PRIVATE (display);
  display->type = GST_VULKAN_DISPLAY_TYPE_ANY;

  g_mutex_init (&display->priv->thread_lock);
  g_cond_init (&display->priv->thread_cond);

  display->priv->event_thread = g_thread_new ("vkdisplay-event",
      (GThreadFunc) _event_thread_main, display);

  g_mutex_lock (&display->priv->thread_lock);
  while (!display->main_loop)
    g_cond_wait (&display->priv->thread_cond, &display->priv->thread_lock);
  g_mutex_unlock (&display->priv->thread_lock);
}

static void
gst_vulkan_display_finalize (GObject * object)
{
  GstVulkanDisplay *display = GST_VULKAN_DISPLAY (object);

  g_mutex_lock (&display->priv->thread_lock);
  if (display->main_context && display->event_source) {
    g_source_destroy (display->event_source);
    g_source_unref (display->event_source);
  }
  display->event_source = NULL;

  if (display->main_loop)
    g_main_loop_quit (display->main_loop);

  while (display->main_loop)
    g_cond_wait (&display->priv->thread_cond, &display->priv->thread_lock);

  if (display->priv->event_thread)
    g_thread_unref (display->priv->event_thread);
  display->priv->event_thread = NULL;
  g_mutex_unlock (&display->priv->thread_lock);

  G_OBJECT_CLASS (gst_vulkan_display_parent_class)->finalize (object);
}

GstVulkanDisplay *
gst_vulkan_display_new_with_type (GstVulkanInstance * instance,
    GstVulkanDisplayType type)
{
  GstVulkanDisplay *display = NULL;

  _init_debug ();

#if GST_VULKAN_HAVE_WINDOW_XCB
  if (!display && type & GST_VULKAN_DISPLAY_TYPE_XCB) {
    display = GST_VULKAN_DISPLAY (gst_vulkan_display_xcb_new (NULL));
  }
#endif

  if (display)
    display->instance = gst_object_ref (instance);

  return display;
}

/**
 * gst_vulkan_display_new:
 *
 * Returns: (transfer full): a new #GstVulkanDisplay
 *
 * Since: 1.10
 */
GstVulkanDisplay *
gst_vulkan_display_new (GstVulkanInstance * instance)
{
  GstVulkanDisplayType type;
  GstVulkanDisplay *display = NULL;

  type = gst_vulkan_display_choose_type (instance);
  display = gst_vulkan_display_new_with_type (instance, type);

  if (!display) {
    /* subclass returned a NULL display */
    GST_FIXME ("creating dummy display");

    display = g_object_new (GST_TYPE_VULKAN_DISPLAY, NULL);
    display->instance = gst_object_ref (instance);
  }

  return display;
}

/**
 * gst_vulkan_display_get_handle:
 * @display: a #GstVulkanDisplay
 *
 * Returns: the winsys specific handle of @display
 *
 * Since: 1.10
 */
gpointer
gst_vulkan_display_get_handle (GstVulkanDisplay * display)
{
  GstVulkanDisplayClass *klass;

  g_return_val_if_fail (GST_IS_VULKAN_DISPLAY (display), NULL);
  klass = GST_VULKAN_DISPLAY_GET_CLASS (display);
  g_return_val_if_fail (klass->get_handle != NULL, NULL);

  return klass->get_handle (display);
}

static gpointer
gst_vulkan_display_default_get_handle (GstVulkanDisplay * display)
{
  return 0;
}

/**
 * gst_vulkan_display_get_handle_type:
 * @display: a #GstVulkanDisplay
 *
 * Returns: the #GstVulkanDisplayType of @display
 *
 * Since: 1.10
 */
GstVulkanDisplayType
gst_vulkan_display_get_handle_type (GstVulkanDisplay * display)
{
  g_return_val_if_fail (GST_IS_VULKAN_DISPLAY (display),
      GST_VULKAN_DISPLAY_TYPE_NONE);

  return display->type;
}

/**
 * gst_vulkan_display_create_window:
 * @display: a #GstVulkanDisplay
 *
 * Returns: a new #GstVulkanWindow for @display or %NULL.
 */
GstVulkanWindow *
gst_vulkan_display_create_window (GstVulkanDisplay * display)
{
  GstVulkanDisplayClass *klass;
  GstVulkanWindow *window;

  g_return_val_if_fail (GST_IS_VULKAN_DISPLAY (display), NULL);
  klass = GST_VULKAN_DISPLAY_GET_CLASS (display);
  g_return_val_if_fail (klass->create_window != NULL, NULL);

  window = klass->create_window (display);

  if (window) {
    GST_OBJECT_LOCK (display);
    display->windows = g_list_prepend (display->windows, window);
    GST_OBJECT_UNLOCK (display);
  }

  return window;
}

static GstVulkanWindow *
gst_vulkan_display_default_create_window (GstVulkanDisplay * display)
{
  return gst_vulkan_window_new (display);
}

gboolean
gst_vulkan_display_remove_window (GstVulkanDisplay * display,
    GstVulkanWindow * window)
{
  gboolean ret = FALSE;
  GList *l;

  GST_OBJECT_LOCK (display);
  l = g_list_find (display->windows, window);
  if (l) {
    display->windows = g_list_delete_link (display->windows, l);
    ret = TRUE;
  }
  GST_OBJECT_UNLOCK (display);

  return ret;
}

/**
 * gst_context_set_vulkan_display:
 * @context: a #GstContext
 * @display: a #GstVulkanDisplay
 *
 * Sets @display on @context
 *
 * Since: 1.10
 */
void
gst_context_set_vulkan_display (GstContext * context,
    GstVulkanDisplay * display)
{
  GstStructure *s;

  g_return_if_fail (context != NULL);
  g_return_if_fail (gst_context_is_writable (context));

  if (display)
    GST_CAT_LOG (GST_CAT_CONTEXT,
        "setting GstVulkanDisplay(%" GST_PTR_FORMAT ") on context(%"
        GST_PTR_FORMAT ")", display, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, GST_VULKAN_DISPLAY_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_DISPLAY, display, NULL);
}

/**
 * gst_context_get_vulkan_display:
 * @context: a #GstContext
 * @display: resulting #GstVulkanDisplay
 *
 * Returns: Whether @display was in @context
 *
 * Since: 1.10
 */
gboolean
gst_context_get_vulkan_display (GstContext * context,
    GstVulkanDisplay ** display)
{
  const GstStructure *s;
  gboolean ret;

  g_return_val_if_fail (display != NULL, FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  s = gst_context_get_structure (context);
  ret = gst_structure_get (s, GST_VULKAN_DISPLAY_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_DISPLAY, display, NULL);

  GST_CAT_LOG (GST_CAT_CONTEXT, "got GstVulkanDisplay(%" GST_PTR_FORMAT
      ") from context(%" GST_PTR_FORMAT ")", *display, context);

  return ret;
}

GstVulkanDisplayType
gst_vulkan_display_choose_type (GstVulkanInstance * instance)
{
  const gchar *window_str;
  GstVulkanDisplayType type = GST_VULKAN_DISPLAY_TYPE_NONE;
  GstVulkanDisplayType first_supported = GST_VULKAN_DISPLAY_TYPE_NONE;

  window_str = g_getenv ("GST_VULKAN_WINDOW");

  /* FIXME: enumerate instance extensions for the supported winsys' */

#define CHOOSE_WINSYS(lname,uname) \
  G_STMT_START { \
    if (!type && g_strcmp0 (window_str, G_STRINGIFY (lname)) == 0) { \
      type = G_PASTE(GST_VULKAN_DISPLAY_TYPE_,uname); \
    } \
    if (!first_supported) \
      first_supported = G_PASTE(GST_VULKAN_DISPLAY_TYPE_,uname); \
  } G_STMT_END

#if GST_VULKAN_HAVE_WINDOW_XCB
  CHOOSE_WINSYS (xcb, XCB);
#endif

#undef CHOOSE_WINSYS

  if (type)
    return type;

  if (first_supported)
    return first_supported;

  return GST_VULKAN_DISPLAY_TYPE_NONE;
}

const gchar *
gst_vulkan_display_type_to_extension_string (GstVulkanDisplayType type)
{
  if (type == GST_VULKAN_DISPLAY_TYPE_NONE)
    return NULL;

  if (type & GST_VULKAN_DISPLAY_TYPE_XCB)
    return VK_KHR_XCB_SURFACE_EXTENSION_NAME;

  return NULL;
}