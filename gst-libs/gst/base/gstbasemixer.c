/* GStreamer base_mixer base class
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <thiagoss@osg.samsung.com>
 *
 * gstbasemixer.c:
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
 * SECTION: gstbasemixer
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstbasemixer.h"


GST_DEBUG_CATEGORY_STATIC (base_mixer_debug);
#define GST_CAT_DEFAULT base_mixer_debug

struct _GstBaseMixerPadPrivate
{
  GstClockTime offset;
};

/***********************************
 * GstBaseMixerPad implementation  *
 ************************************/
G_DEFINE_TYPE (GstBaseMixerPad, gst_base_mixer_pad, GST_TYPE_AGGREGATOR_PAD);

static void
gst_base_mixer_pad_class_init (GstBaseMixerPadClass * klass)
{
  g_type_class_add_private (klass, sizeof (GstBaseMixerPadPrivate));
}

static void
gst_base_mixer_pad_init (GstBaseMixerPad * pad)
{
  pad->priv =
      G_TYPE_INSTANCE_GET_PRIVATE (pad, GST_TYPE_BASE_MIXER_PAD,
      GstBaseMixerPadPrivate);
}

static void
gst_base_mixer_pad_advance (GstBaseMixerPad * mixerpad, GstClockTime duration)
{
  GstBuffer *buffer;

  GST_LOG_OBJECT (mixerpad, "Advancing: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (duration));

  buffer = gst_aggregator_pad_get_buffer (GST_AGGREGATOR_PAD_CAST (mixerpad));
  if (buffer) {
    mixerpad->priv->offset += duration;
    if (GST_BUFFER_DURATION_IS_VALID (buffer)) {

      g_assert (GST_BUFFER_DURATION (buffer) >= mixerpad->priv->offset);

      /* It should never be smaller but let's leave it here for safety */
      if (GST_BUFFER_DURATION (buffer) <= mixerpad->priv->offset) {
        gst_aggregator_pad_drop_buffer (GST_AGGREGATOR_PAD_CAST (mixerpad));
        mixerpad->priv->offset = 0;
      }
    }
    gst_buffer_unref (buffer);
  }
}

/*************************************
 * GstBaseMixer implementation  *
 *************************************/
static GstElementClass *base_mixer_parent_class = NULL;

/* All members are protected by the object lock unless otherwise noted */

struct _GstBaseMixerPrivate
{
  gint non_empty;
};

typedef struct _GstBaseMixerTimeAlignment
{
  GstClockTime start;
  GstClockTime end;
} GstBaseMixerTimeAlignment;

static gboolean
gst_base_mixer_find_time_alignment_foreach (GstAggregator * agg,
    GstAggregatorPad * aggpad, gpointer udata)
{
  GstBaseMixerPad *mixerpad = GST_BASE_MIXER_PAD_CAST (aggpad);
  GstBaseMixerTimeAlignment *talign = udata;
  GstBuffer *buf;

  /* TODO handle GST_CLOCK_TIME_NONE */
  /* TODO check if segment/buffer in aggregator is racy */
  /* TODO do we need to lock the pad here? */
  /* TODO do we need to translate the timestamps here to the output segment?
   *      or is it always good as it is based on running time? */

  GST_LOG_OBJECT (aggpad,
      "Checking pad data. Current offset: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (mixerpad->priv->offset));

  buf = gst_aggregator_pad_get_buffer (aggpad);
  if (buf) {
    GstClockTime start = GST_CLOCK_TIME_NONE, end = GST_CLOCK_TIME_NONE;
    if (GST_BUFFER_PTS_IS_VALID (buf)) {
      start =
          gst_segment_to_running_time (&aggpad->segment, GST_FORMAT_TIME,
          GST_BUFFER_PTS (buf) + mixerpad->priv->offset);
      if (GST_BUFFER_DURATION_IS_VALID (buf)) {
        end = start + GST_BUFFER_DURATION (buf) - mixerpad->priv->offset;       /* TODO clip on segment */

        if (end < talign->end)
          talign->end = end;
      }

      if (start < talign->start)
        talign->start = start;
    }
    GST_DEBUG_OBJECT (aggpad,
        "Pad has times: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
        GST_TIME_ARGS (start), GST_TIME_ARGS (end));
    gst_buffer_unref (buf);
  }

  return TRUE;
}

static void
gst_base_mixer_find_time_alignment (GstBaseMixer * bmixer, GstClockTime * start,
    GstClockTime * end)
{
  GstBaseMixerTimeAlignment timealign;

  timealign.start = GST_CLOCK_TIME_NONE;
  timealign.end = GST_CLOCK_TIME_NONE;

  GST_DEBUG_OBJECT (bmixer, "Starting time alignment");

  gst_aggregator_iterate_sinkpads (GST_AGGREGATOR_CAST (bmixer),
      gst_base_mixer_find_time_alignment_foreach, &timealign);
  *start = timealign.start;
  *end = timealign.end;

  GST_DEBUG_OBJECT (bmixer,
      "Time aligned data to: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
      GST_TIME_ARGS (*start), GST_TIME_ARGS (*end));
}

static gboolean
gst_base_mixer_advance_foreach (GstAggregator * agg, GstAggregatorPad * aggpad,
    gpointer udata)
{
  GstBaseMixerPad *mixerpad = GST_BASE_MIXER_PAD_CAST (aggpad);
  GstClockTime *duration = udata;

  gst_base_mixer_pad_advance (mixerpad, *duration);
  return TRUE;
}

static void
gst_base_mixer_advance (GstBaseMixer * bmixer, GstClockTime duration)
{
  gst_aggregator_iterate_sinkpads (GST_AGGREGATOR_CAST (bmixer),
      gst_base_mixer_advance_foreach, &duration);
}

GstFlowReturn
gst_base_mixer_finish_buffer (GstBaseMixer * bmixer, GstBuffer * buffer)
{
  GstFlowReturn ret;
  GstClockTime dur;

  dur = GST_BUFFER_DURATION (buffer);

  ret = gst_aggregator_finish_buffer (GST_AGGREGATOR_CAST (bmixer), buffer);
  gst_base_mixer_advance (bmixer, dur);

  return ret;
}

static GstFlowReturn
gst_base_mixer_mix (GstBaseMixer * bmixer, GstClockTime start, GstClockTime end)
{
  GstBaseMixerClass *bmixer_class = GST_BASE_MIXER_GET_CLASS (bmixer);

  g_return_val_if_fail (bmixer_class->mix != NULL, GST_FLOW_ERROR);

  return bmixer_class->mix (bmixer, start, end);
}

static GstFlowReturn
gst_base_mixer_prepare (GstBaseMixer * bmixer, gboolean timeout)
{
  GstBaseMixerClass *bmixer_class = GST_BASE_MIXER_GET_CLASS (bmixer);

  if (bmixer_class->prepare)
    return bmixer_class->prepare (bmixer, timeout);
  return GST_FLOW_OK;
}

/* TODO write unit tests for this feature */
/* FIXME - design: by reducing the 'end' time the class will be
 * forced to mix again the same data, which will likely waste CPU.
 * It might make more sense to make subclasses push multiple buffers
 * from the same mix() call as it would be possible to push the same
 * buffer reference over again */
static void
gst_base_mixer_adjust_times (GstBaseMixer * bmixer, GstClockTime * start,
    GstClockTime * end)
{
  GstBaseMixerClass *bmixer_class = GST_BASE_MIXER_GET_CLASS (bmixer);

  if (bmixer_class->adjust_times)
    bmixer_class->adjust_times (bmixer, start, end);
}

static GstFlowReturn
gst_base_mixer_aggregate (GstAggregator * agg, gboolean timeout)
{
  GstBaseMixer *bmixer = GST_BASE_MIXER_CAST (agg);
  GstClockTime start, end;
  GstClockTime suggested_start, suggested_end;
  GstFlowReturn ret;

  ret = gst_base_mixer_prepare (bmixer, timeout);
  if (ret != GST_FLOW_OK)
    return ret;

  gst_base_mixer_find_time_alignment (bmixer, &start, &end);
  suggested_start = start;
  suggested_end = end;

  /* check if subclass wants a different mixing time */
  gst_base_mixer_adjust_times (bmixer, &start, &end);

  /* Subclass should only increase start and/or decrease end,
   * Decreasing start means returning in time and we might not
   * have that data anymore.
   * Increasing end will go beyond the mixing limit of the current
   * data and will lead to incorrect results */
  g_assert (start >= suggested_start);
  g_assert (end <= suggested_end);

  if (start > suggested_start) {
    /* TODO implement this -
     * subclass wants to skip some data */
  }

  ret = gst_base_mixer_mix (bmixer, start, end);

  return ret;
}

static void
gst_base_mixer_finalize (GObject * object)
{
  G_OBJECT_CLASS (base_mixer_parent_class)->finalize (object);
}

/* GObject vmethods implementations */
static void
gst_base_mixer_class_init (GstBaseMixerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstAggregatorClass *aggregator_class = (GstAggregatorClass *) klass;

  base_mixer_parent_class = g_type_class_peek_parent (klass);
  g_type_class_add_private (klass, sizeof (GstBaseMixerPrivate));

  GST_DEBUG_CATEGORY_INIT (base_mixer_debug, "base_mixer",
      GST_DEBUG_FG_MAGENTA, "GstBaseMixer");

  gobject_class->finalize = gst_base_mixer_finalize;

  aggregator_class->aggregate = gst_base_mixer_aggregate;

  aggregator_class->sinkpads_type = GST_TYPE_BASE_MIXER_PAD;
}

static void
gst_base_mixer_init (GstBaseMixer * self, GstBaseMixerClass * klass)
{
  self->priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_BASE_MIXER,
      GstBaseMixerPrivate);
}

/* we can't use G_DEFINE_ABSTRACT_TYPE because we need the klass in the _init
 * method to get to the padtemplates */
GType
gst_base_mixer_get_type (void)
{
  static volatile gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstBaseMixerClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_base_mixer_class_init,
      NULL,
      NULL,
      sizeof (GstBaseMixer),
      0,
      (GInstanceInitFunc) gst_base_mixer_init,
    };

    _type = g_type_register_static (GST_TYPE_AGGREGATOR,
        "GstBaseMixer", &info, G_TYPE_FLAG_ABSTRACT);
    g_once_init_leave (&type, _type);
  }
  return type;
}
