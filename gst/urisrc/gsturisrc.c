/* GStreamer
 *
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <ts.santos@sisa.samsung.com>
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

/**
 * SECTION:element-urisrc
 *
 * urisrc is a bin that has a internal source for a given URI and can
 * internally switch to a new URI after the current one goes EOS.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsturisrc.h"

#include <string.h>
#include <gst/gst-i18n-plugin.h>

GST_DEBUG_CATEGORY (uri_src_debug);
#define GST_CAT_DEFAULT (uri_src_debug)

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,
  PROP_URI,
};

static void gst_uri_src_finalize (GObject * object);
static void gst_uri_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_uri_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn
gst_uri_src_change_state (GstElement * element, GstStateChange trans);

#define gst_uri_src_parent_class parent_class
G_DEFINE_TYPE (GstUriSrc, gst_uri_src, GST_TYPE_BIN);

static void
gst_uri_src_class_init (GstUriSrcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_uri_src_finalize;
  gobject_class->set_property = gst_uri_src_set_property;
  gobject_class->get_property = gst_uri_src_get_property;

  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI",
          "URI that should be used",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_set_static_metadata (element_class,
      "URI source bin", "Source/Bin", "Handles switching internal "
      "sources that handle URIs", "Thiago Santos <ts.santos@sisa.samsung.com>");

  GST_DEBUG_CATEGORY_INIT (uri_src_debug, "urisrc", 0, "URI source bin");

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_uri_src_change_state);
}

static void
gst_uri_src_init (GstUriSrc * urisrc)
{
  GstPadTemplate *tmpl;

  tmpl = gst_static_pad_template_get (&src_template);
  urisrc->srcpad = gst_ghost_pad_new_no_target_from_template ("src", tmpl);
  gst_object_unref (tmpl);

  gst_element_add_pad (GST_ELEMENT_CAST (urisrc), urisrc->srcpad);
}

static void
gst_uri_src_finalize (GObject * object)
{
  GstUriSrc *src = GST_URI_SRC (object);

  g_free (src->uri);
  src->uri = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_uri_src_update_src (GstUriSrc * urisrc)
{
  GST_OBJECT_LOCK (urisrc);
  if (urisrc->src) {
    gchar *old_protocol, *new_protocol;
    gchar *old_uri;

    old_uri = gst_uri_handler_get_uri (GST_URI_HANDLER (urisrc->src));
    old_protocol = gst_uri_get_protocol (old_uri);
    new_protocol = gst_uri_get_protocol (urisrc->uri);

    if (!g_str_equal (old_protocol, new_protocol)) {
      gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (urisrc->srcpad), NULL);
      gst_element_set_state (urisrc->src, GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (urisrc), urisrc->src);
      urisrc->src = NULL;
      GST_DEBUG_OBJECT (urisrc, "Can't re-use old source element");
    } else {
      GError *err = NULL;

      GST_DEBUG_OBJECT (urisrc, "Re-using old source element");
      if (!gst_uri_handler_set_uri (GST_URI_HANDLER (urisrc->src),
              urisrc->uri, &err)) {
        GST_DEBUG_OBJECT (urisrc, "Failed to re-use old source element: %s",
            err->message);
        g_clear_error (&err);
        gst_element_set_state (urisrc->src, GST_STATE_NULL);
        gst_object_unref (urisrc->src);
        urisrc->src = NULL;
      }
    }
    g_free (old_uri);
    g_free (old_protocol);
    g_free (new_protocol);
  }

  if (!urisrc->src) {
    GST_DEBUG_OBJECT (urisrc, "Creating source element for the URI:%s",
        urisrc->uri);
    urisrc->src =
        gst_element_make_from_uri (GST_URI_SRC, urisrc->uri, NULL, NULL);
    if (urisrc->src) {
      GstPad *pad = gst_element_get_static_pad (urisrc->src, "src");

      gst_bin_add (GST_BIN_CAST (urisrc), urisrc->src);
      gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (urisrc->srcpad), pad);
      gst_object_unref (pad);
      gst_element_sync_state_with_parent (urisrc->src);
    } else {
      GST_ELEMENT_ERROR (urisrc, CORE, MISSING_PLUGIN,
          (_("No URI handler implemented for \"%s\"."), urisrc->uri), (NULL));
    }
  }
  GST_OBJECT_UNLOCK (urisrc);
}

static void
gst_uri_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstUriSrc *urisrc = GST_URI_SRC (object);

  switch (prop_id) {
    case PROP_URI:{
      GThread *thread;

      urisrc->uri = g_value_dup_string (value);
      /* FIXME if we set this fast enough can the order of urisrc get
       * messed up? */
      thread =
          g_thread_new (NULL, (GThreadFunc) gst_uri_src_update_src, urisrc);
      g_thread_unref (thread);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_uri_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstUriSrc *urisrc = GST_URI_SRC (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string (value, urisrc->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_uri_src_change_state (GstElement * element, GstStateChange trans)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  /* GstUriSrc *urisrc = GST_URI_SRC_CAST (element); */

  switch (trans) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  switch (trans) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "urisrc",
      GST_RANK_PRIMARY, GST_TYPE_URI_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    urisrc,
    "URI source bin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
