/*
 * HDS demux plugin for GStreamer
 *
 * gsthdsdemux.c
 *
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <thiagoss@osg.samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/**
 * SECTION:element-hdsdemux
 *
 * HDS demuxer element.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <inttypes.h>
#include <gst/tag/tag.h>
#include "gst/gst-i18n-plugin.h"
#include "gsthdsdemux.h"
#include "gsthds_debug.h"

/* FIXME should we enable this? How do we handle elements that
 * can provide video, audio or video+audio (container), use
 * only src_%02u or have video_%02u and audio_%02u as well?
 *
static GstStaticPadTemplate gst_hds_demux_videosrc_template =
GST_STATIC_PAD_TEMPLATE ("video_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_hds_demux_audiosrc_template =
GST_STATIC_PAD_TEMPLATE ("audio_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);
*/

static GstStaticPadTemplate gst_hds_demux_src_template =
GST_STATIC_PAD_TEMPLATE ("src_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);


static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/f4m"));

GST_DEBUG_CATEGORY (gst_hds_demux_debug);
#define GST_CAT_DEFAULT gst_hds_demux_debug

/* GObject */
static void gst_hds_demux_dispose (GObject * obj);

/* GstAdaptiveDemux */
static GstClockTime gst_hds_demux_get_duration (GstAdaptiveDemux * ademux);
static void gst_hds_demux_reset (GstAdaptiveDemux * ademux);
static gboolean gst_hds_demux_process_manifest (GstAdaptiveDemux * ademux,
    GstBuffer * buf);
static GstFlowReturn
gst_hds_demux_stream_update_fragment_info (GstAdaptiveDemuxStream * stream);
static GstFlowReturn
gst_hds_demux_stream_advance_fragment (GstAdaptiveDemuxStream * stream);
static gboolean gst_hds_demux_stream_select_bitrate (GstAdaptiveDemuxStream *
    stream, guint64 bitrate);

#define gst_hds_demux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstHdsDemux, gst_hds_demux, GST_TYPE_ADAPTIVE_DEMUX,
    GST_DEBUG_CATEGORY_INIT (gst_hds_demux_debug, "hdsdemux", 0,
        "hdsdemux element");
    );

static void
gst_hds_demux_dispose (GObject * obj)
{
  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_hds_demux_class_init (GstHdsDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAdaptiveDemuxClass *gstadaptivedemux_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstadaptivedemux_class = (GstAdaptiveDemuxClass *) klass;

  gobject_class->dispose = gst_hds_demux_dispose;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_hds_demux_src_template));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));

  gst_element_class_set_static_metadata (gstelement_class,
      "HDS Demuxer",
      "Codec/Demuxer/Adaptive",
      "HTTP Dynamic Streaming over HTTP demuxer",
      "Thiago Santos <thiagoss@osg.samsung.com>");

  gstadaptivedemux_class->get_duration = gst_hds_demux_get_duration;
  gstadaptivedemux_class->reset = gst_hds_demux_reset;
  gstadaptivedemux_class->process_manifest = gst_hds_demux_process_manifest;
  gstadaptivedemux_class->stream_advance_fragment =
      gst_hds_demux_stream_advance_fragment;
  gstadaptivedemux_class->stream_select_bitrate =
      gst_hds_demux_stream_select_bitrate;
  gstadaptivedemux_class->stream_update_fragment_info =
      gst_hds_demux_stream_update_fragment_info;
}

static void
gst_hds_demux_init (GstHdsDemux * demux)
{
  gst_adaptive_demux_set_stream_struct_size (GST_ADAPTIVE_DEMUX_CAST (demux),
      sizeof (GstHdsDemuxStream));
}

static GstClockTime
gst_hds_demux_get_duration (GstAdaptiveDemux * ademux)
{
  return GST_CLOCK_TIME_NONE;
}

static gboolean
gst_hds_demux_process_manifest (GstAdaptiveDemux * demux, GstBuffer * buf)
{
  GstHdsDemux *hdsdemux = GST_HDS_DEMUX_CAST (demux);
  gchar *manifest;
  GstMapInfo mapinfo;

  if (hdsdemux->client)
    gst_hds_client_free (hdsdemux->client);
  hdsdemux->client = gst_hds_client_new ();

  if (gst_buffer_map (buf, &mapinfo, GST_MAP_READ)) {
    manifest = (gchar *) mapinfo.data;
    if (gst_hds_client_parse_manifest (hdsdemux->client, manifest,
            mapinfo.size)) {
    }
    gst_buffer_unmap (buf, &mapinfo);
  } else {
    GST_WARNING_OBJECT (demux, "Failed to map manifest buffer");
  }

  return FALSE;
}

static void
gst_hds_demux_reset (GstAdaptiveDemux * ademux)
{
}

static GstFlowReturn
gst_hds_demux_stream_update_fragment_info (GstAdaptiveDemuxStream * stream)
{
  return GST_FLOW_EOS;
}

static GstFlowReturn
gst_hds_demux_stream_advance_fragment (GstAdaptiveDemuxStream * stream)
{
  return GST_FLOW_ERROR;
}

static gboolean
gst_hds_demux_stream_select_bitrate (GstAdaptiveDemuxStream * stream,
    guint64 bitrate)
{
  return FALSE;
}
