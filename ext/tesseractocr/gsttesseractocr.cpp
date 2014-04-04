/* GStreamer
 * Copyright (C) 2014 Thiago Santos <ts.santos@sisa.samsung.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gsttesseractocr
 *
 * The tesseractocr element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! tesseractocr ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gsttesseractocr.h"

GST_DEBUG_CATEGORY_STATIC (gst_tesseract_ocr_debug_category);
#define GST_CAT_DEFAULT gst_tesseract_ocr_debug_category

/* prototypes */

static void gst_tesseract_ocr_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_tesseract_ocr_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_tesseract_ocr_dispose (GObject * object);
static void gst_tesseract_ocr_finalize (GObject * object);

static GstStateChangeReturn
gst_tesseract_ocr_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_tesseract_ocr_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_tesseract_ocr_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_tesseract_ocr_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, format=(string)GRAY8")
    );

static GstStaticPadTemplate gst_tesseract_ocr_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/raw, format=(string)utf8")
    );

/* class initialization */
#define parent_class gst_tesseract_ocr_parent_class
G_DEFINE_TYPE_WITH_CODE (GstTesseractOcr, gst_tesseract_ocr, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_tesseract_ocr_debug_category, "tesseractocr",
        0, "debug category for tesseractocr element"));

static void
gst_tesseract_ocr_class_init (GstTesseractOcrClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_tesseract_ocr_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_tesseract_ocr_src_template));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Tesseract OCR", "Text/OCR/Converter", "Runs OCR in images and"
      " outputs the found text", "Thiago Santos <ts.santos@sisa.samsung.com>");

  gobject_class->set_property = gst_tesseract_ocr_set_property;
  gobject_class->get_property = gst_tesseract_ocr_get_property;
  gobject_class->dispose = gst_tesseract_ocr_dispose;
  gobject_class->finalize = gst_tesseract_ocr_finalize;
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_tesseract_ocr_change_state);
}

static void
gst_tesseract_ocr_init (GstTesseractOcr * tesseractocr)
{
  tesseractocr->sinkpad =
      gst_pad_new_from_static_template (&gst_tesseract_ocr_sink_template,
      "sink");
  gst_pad_set_event_function (tesseractocr->sinkpad,
      GST_DEBUG_FUNCPTR (gst_tesseract_ocr_sink_event));
  gst_pad_set_chain_function (tesseractocr->sinkpad,
      GST_DEBUG_FUNCPTR (gst_tesseract_ocr_sink_chain));
  gst_element_add_pad (GST_ELEMENT (tesseractocr), tesseractocr->sinkpad);

  tesseractocr->srcpad =
      gst_pad_new_from_static_template (&gst_tesseract_ocr_src_template, "src");
  gst_element_add_pad (GST_ELEMENT (tesseractocr), tesseractocr->srcpad);
}

void
gst_tesseract_ocr_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTesseractOcr *tesseractocr = GST_TESSERACT_OCR (object);

  GST_DEBUG_OBJECT (tesseractocr, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_tesseract_ocr_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstTesseractOcr *tesseractocr = GST_TESSERACT_OCR (object);

  GST_DEBUG_OBJECT (tesseractocr, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_tesseract_ocr_dispose (GObject * object)
{
  GstTesseractOcr *tesseractocr = GST_TESSERACT_OCR (object);

  GST_DEBUG_OBJECT (tesseractocr, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_tesseract_ocr_parent_class)->dispose (object);
}

void
gst_tesseract_ocr_finalize (GObject * object)
{
  GstTesseractOcr *tesseractocr = GST_TESSERACT_OCR (object);

  GST_DEBUG_OBJECT (tesseractocr, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_tesseract_ocr_parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_tesseract_ocr_change_state (GstElement * element, GstStateChange transition)
{
  GstTesseractOcr *tesseractocr;
  GstStateChangeReturn ret;

  g_return_val_if_fail (GST_IS_TESSERACT_OCR (element),
      GST_STATE_CHANGE_FAILURE);
  tesseractocr = GST_TESSERACT_OCR (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      tesseractocr->api = new tesseract::TessBaseAPI ();
      if (tesseractocr->api->Init (NULL, "eng")) {
        GST_ERROR_OBJECT (tesseractocr, "Could not initialize tesseract");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_event_replace (&tesseractocr->pending_segment, NULL);
      tesseractocr->vinfo.finfo = NULL;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      tesseractocr->api->End ();
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_tesseract_ocr_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstTesseractOcr *tesseractocr;
  gboolean ret = TRUE;

  tesseractocr = GST_TESSERACT_OCR (parent);

  GST_DEBUG_OBJECT (tesseractocr, "event %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_video_info_from_caps (&tesseractocr->vinfo, caps);
      gst_event_unref (event);
      event = NULL;
      break;
    case GST_EVENT_SEGMENT:
      gst_event_replace (&tesseractocr->pending_segment, event);
      gst_event_unref (event);
      event = NULL;
      break;
    default:
      break;
  }

  if (event)
    return gst_pad_event_default (pad, parent, event);
  return ret;
}

static GstFlowReturn
gst_tesseract_ocr_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstTesseractOcr *tesseractocr;
  GstMapInfo info;
  gchar *text;
  GstBuffer *out_buffer;

  tesseractocr = GST_TESSERACT_OCR (parent);

  if (G_UNLIKELY (!gst_pad_has_current_caps (tesseractocr->srcpad))) {
    if (!gst_pad_set_caps (tesseractocr->srcpad, gst_caps_new_simple (
        "text/raw", "format", G_TYPE_STRING, "utf8", NULL))) {
      gst_buffer_unref (buffer);
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  if (G_UNLIKELY (tesseractocr->pending_segment)) {
    gst_pad_push_event (tesseractocr->srcpad, tesseractocr->pending_segment);
    tesseractocr->pending_segment = NULL;
  }

  gst_buffer_map (buffer, &info, GST_MAP_READ);

  tesseractocr->api->SetImage (info.data, tesseractocr->vinfo.width,
      tesseractocr->vinfo.height,
      GST_VIDEO_FORMAT_INFO_BITS (tesseractocr->vinfo.finfo) / 8,
      GST_VIDEO_INFO_PLANE_STRIDE (&tesseractocr->vinfo, 0));

  text = tesseractocr->api->GetUTF8Text ();
  out_buffer = gst_buffer_new_wrapped (text, strlen (text));
  /* TODO this 'text' variable should be freed with delete[]
   * according to tesseract docs */

  GST_DEBUG_OBJECT (tesseractocr, "found text '%s'", text);

  gst_buffer_unmap (buffer, &info);
  return gst_pad_push (tesseractocr->srcpad, out_buffer);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "tesseractocr", GST_RANK_NONE,
      GST_TYPE_TESSERACT_OCR);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    tesseractocr,
    "tesseract ocr utilities plugin",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
