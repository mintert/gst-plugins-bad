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

typedef struct _GstBaseMixerTimeAlignment
{
  GstClockTime start;
  GstClockTime end;
} GstBaseMixerTimeAlignment;

struct _GstBaseMixerPadPrivate
{
  GstClockTime offset;
  GstClockTime clipped_duration;
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

gboolean
gst_base_mixer_pad_is_eos (GstBaseMixerPad * mixerpad)
{
  return gst_aggregator_pad_is_eos (GST_AGGREGATOR_PAD_CAST (mixerpad))
      && mixerpad->buffer == NULL;
}

static void
gst_base_mixer_pad_drop_buffer (GstBaseMixerPad * mixerpad)
{
  mixerpad->priv->offset = 0;
  gst_buffer_unref (mixerpad->buffer);
  mixerpad->buffer = NULL;
}

static gboolean
gst_base_mixer_pad_get_times (GstBaseMixerPad * mixerpad, GstClockTime * start,
    GstClockTime * end)
{
  if (!mixerpad->buffer)
    return FALSE;

  *start = mixerpad->buffer_start_ts + mixerpad->priv->offset;
  *end = mixerpad->buffer_end_ts;
  GST_DEBUG_OBJECT (mixerpad,
      "Pad has times: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
      GST_TIME_ARGS (*start), GST_TIME_ARGS (*end));
  return TRUE;
}

static GstClockTime
gst_base_mixer_pad_get_duration_within_times (GstBaseMixerPad * mixerpad,
    GstClockTime start, GstClockTime end)
{
  GstClockTime buf_start, buf_end;

  gst_base_mixer_pad_get_times (mixerpad, &buf_start, &buf_end);

  /* we are advancing a time before our own buffer. Nothing to advance for us */
  if (end <= buf_start) {
    GST_LOG_OBJECT (mixerpad, "Our buffer is ahead of the current mix time."
        " No advancing needed");
    return 0;
  }

  /* TODO verify if this is possible, and likely add unit tests for it */
  if (buf_end <= start) {
    GST_LOG_OBJECT (mixerpad, "Our buffer is behind the mixing time, "
        "advance it all");
    return buf_end - buf_start;
  }

  return MIN (end, buf_end) - buf_start;
}

static void
gst_base_mixer_pad_advance (GstBaseMixerPad * mixerpad, GstClockTime start,
    GstClockTime end)
{
  GstClockTime duration = 0;

  GST_LOG_OBJECT (mixerpad,
      "Advancing: %" GST_TIME_FORMAT "- %" GST_TIME_FORMAT,
      GST_TIME_ARGS (start), GST_TIME_ARGS (end));

  if (!mixerpad->buffer) {
    GST_DEBUG_OBJECT (mixerpad, "Has no buffer, no advance needed");
    return;
  }

  duration =
      gst_base_mixer_pad_get_duration_within_times (mixerpad, start, end);
  GST_DEBUG_OBJECT (mixerpad, "Advancing duration: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (duration));
  mixerpad->priv->offset += duration;
  if (GST_CLOCK_TIME_IS_VALID (mixerpad->buffer_end_ts)) {
    g_assert (mixerpad->buffer_end_ts >=
        mixerpad->buffer_start_ts + mixerpad->priv->offset);

    /* It should never be smaller but let's leave it here for safety */
    if (mixerpad->buffer_end_ts <=
        mixerpad->buffer_start_ts + mixerpad->priv->offset) {
      gst_base_mixer_pad_drop_buffer (mixerpad);
    }
  }
}


/*************************************
 * GstBaseMixer implementation  *
 *************************************/
static GstElementClass *base_mixer_parent_class = NULL;

/* All members are protected by the object lock unless otherwise noted */

struct _GstBaseMixerPrivate
{
  GstFlowReturn prepare_result;
};

static gboolean
gst_base_mixer_find_time_alignment_foreach (GstAggregator * agg,
    GstAggregatorPad * aggpad, gpointer udata)
{
  GstBaseMixerPad *mixerpad = GST_BASE_MIXER_PAD_CAST (aggpad);
  GstBaseMixerTimeAlignment *talign = udata;
  GstClockTime start, end;

  /* TODO handle GST_CLOCK_TIME_NONE */
  /* TODO check if segment/buffer in aggregator is racy */
  /* TODO do we need to lock the pad here? */
  /* TODO do we need to translate the timestamps here to the output segment?
   *      or is it always good as it is based on running time? */
  /* TODO skip data that is late (pads that joined after stream has started */

  GST_LOG_OBJECT (aggpad,
      "Checking pad data. Current offset: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (mixerpad->priv->offset));

  if (gst_base_mixer_pad_get_times (mixerpad, &start, &end)) {
    if (GST_CLOCK_TIME_IS_VALID (start)) {
      if (start < talign->start)
        talign->start = start;

      if (GST_CLOCK_TIME_IS_VALID (end)) {
        if (end < talign->end)
          talign->end = end;
      }
    }
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
  GstBaseMixerTimeAlignment *t = udata;

  gst_base_mixer_pad_advance (mixerpad, t->start, t->end);
  return TRUE;
}

static void
gst_base_mixer_advance (GstBaseMixer * bmixer, GstClockTime ts,
    GstClockTime duration)
{
  GstBaseMixerTimeAlignment t;

  t.start = ts;
  if (GST_CLOCK_TIME_IS_VALID (duration)) {
    t.end = ts + duration;
  } else {
    t.end = GST_CLOCK_TIME_NONE;
  }
  gst_aggregator_iterate_sinkpads (GST_AGGREGATOR_CAST (bmixer),
      gst_base_mixer_advance_foreach, &t);
}

GstFlowReturn
gst_base_mixer_finish_buffer (GstBaseMixer * bmixer, GstBuffer * buffer)
{
  GstFlowReturn ret;
  GstClockTime ts, dur;

  ts = GST_BUFFER_PTS (buffer);
  dur = GST_BUFFER_DURATION (buffer);

  ret = gst_aggregator_finish_buffer (GST_AGGREGATOR_CAST (bmixer), buffer);
  gst_base_mixer_advance (bmixer, ts, dur);

  return ret;
}

static GstFlowReturn
gst_base_mixer_mix (GstBaseMixer * bmixer, GstClockTime start, GstClockTime end)
{
  GstBaseMixerClass *bmixer_class = GST_BASE_MIXER_GET_CLASS (bmixer);

  g_return_val_if_fail (bmixer_class->mix != NULL, GST_FLOW_ERROR);

  GST_DEBUG_OBJECT (bmixer, "Starting mix");

  return bmixer_class->mix (bmixer, start, end);
}

static gboolean
gst_base_mixer_pad_prepare (GstAggregator * agg,
    GstAggregatorPad * aggpad, gpointer udata)
{
  GstBaseMixer *bmixer = GST_BASE_MIXER_CAST (agg);
  GstBaseMixerPad *mixerpad = GST_BASE_MIXER_PAD_CAST (aggpad);
  gboolean timeout = GPOINTER_TO_INT (udata);

restart:
  /* Get a new buffer if needed */
  if (!mixerpad->buffer) {

    mixerpad->buffer = gst_aggregator_pad_steal_buffer (aggpad);
    if (!mixerpad->buffer) {
      if (timeout || gst_aggregator_pad_is_eos (aggpad))
        return TRUE;
      bmixer->priv->prepare_result = GST_FLOW_NOT_HANDLED;
      return FALSE;
    }

    if (!GST_BUFFER_PTS_IS_VALID (mixerpad->buffer)) {
      GST_WARNING_OBJECT (aggpad, "Need timestamped buffers");
      bmixer->priv->prepare_result = GST_FLOW_ERROR;
      return FALSE;
    }

    mixerpad->buffer_start_ts = GST_CLOCK_TIME_NONE;
    mixerpad->buffer_end_ts = GST_CLOCK_TIME_NONE;
  }

  if (!GST_CLOCK_TIME_IS_VALID (mixerpad->buffer_start_ts)) {
    GstClockTime ts, ts_end, c_ts, c_ts_end, dur;
    GstSegment *segment;

    segment = &aggpad->segment;
    ts = GST_BUFFER_PTS (mixerpad->buffer);
    dur = GST_BUFFER_DURATION (mixerpad->buffer);

    if (GST_CLOCK_TIME_IS_VALID (dur))
      ts_end = ts + dur;
    else
      ts_end = GST_CLOCK_TIME_NONE;

    if (!gst_segment_clip (segment, GST_FORMAT_TIME, ts, ts_end, &c_ts,
            &c_ts_end)) {
      GST_DEBUG_OBJECT (aggpad, "Buffer is out of segment, dropping");
      gst_base_mixer_pad_drop_buffer (mixerpad);
      goto restart;
    }

    mixerpad->buffer_start_ts = gst_segment_to_running_time (segment,
        GST_FORMAT_TIME, c_ts);
    if (GST_CLOCK_TIME_IS_VALID (c_ts_end))
      mixerpad->buffer_end_ts = gst_segment_to_running_time (segment,
          GST_FORMAT_TIME, c_ts_end);
  }

  return TRUE;
}

/* Make sure we have a buffer on each pad (if not timeout).
 * For each pad with a buffer get the start and end time of
 * the buffers */
static GstFlowReturn
gst_base_mixer_prepare (GstBaseMixer * bmixer, gboolean timeout)
{
  GstBaseMixerClass *bmixer_class = GST_BASE_MIXER_GET_CLASS (bmixer);

  bmixer->priv->prepare_result = GST_FLOW_OK;

  gst_aggregator_iterate_sinkpads (GST_AGGREGATOR_CAST (bmixer),
      gst_base_mixer_pad_prepare, GINT_TO_POINTER (timeout));
  if (bmixer->priv->prepare_result != GST_FLOW_OK) {
    return bmixer->priv->prepare_result;
  }

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
  if (ret != GST_FLOW_OK) {
    /* means we have to try again */
    if (ret == GST_FLOW_NOT_HANDLED)
      ret = GST_FLOW_OK;

    return ret;
  }

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

static gboolean
gst_base_mixer_check_eos_foreach (GstAggregator * agg,
    GstAggregatorPad * aggpad, gpointer udata)
{
  gboolean *ret = udata;

  if (!gst_base_mixer_pad_is_eos (GST_BASE_MIXER_PAD_CAST (aggpad))) {
    *ret = FALSE;
    return FALSE;
  }
  return TRUE;
}

gboolean
gst_base_mixer_check_eos (GstBaseMixer * bmixer)
{
  gboolean ret = TRUE;

  gst_aggregator_iterate_sinkpads (GST_AGGREGATOR_CAST (bmixer),
      gst_base_mixer_check_eos_foreach, &ret);

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
