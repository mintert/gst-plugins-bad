/* GStreamer base mixer class
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <thiagoss@osg.samsung.com>
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

#ifndef __GST_BASE_MIXER_H__
#define __GST_BASE_MIXER_H__

#ifndef GST_USE_UNSTABLE_API
#warning "The Base library from gst-plugins-bad is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <gst/gst.h>
#include "gstaggregator.h"

G_BEGIN_DECLS

/**************************
 * GstBaseMixer Structs  *
 *************************/

typedef struct _GstBaseMixer GstBaseMixer;
typedef struct _GstBaseMixerPrivate GstBaseMixerPrivate;
typedef struct _GstBaseMixerClass GstBaseMixerClass;

/************************
 * GstBaseMixerPad API *
 ***********************/

#define GST_TYPE_BASE_MIXER_PAD            (gst_base_mixer_pad_get_type())
#define GST_BASE_MIXER_PAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_MIXER_PAD, GstBaseMixerPad))
#define GST_BASE_MIXER_PAD_CAST(obj)       ((GstBaseMixerPad *)(obj))
#define GST_BASE_MIXER_PAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_MIXER_PAD, GstBaseMixerPadClass))
#define GST_BASE_MIXER_PAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_BASE_MIXER_PAD, GstBaseMixerPadClass))
#define GST_IS_BASE_MIXER_PAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_MIXER_PAD))
#define GST_IS_BASE_MIXER_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_MIXER_PAD))

/****************************
 * GstBaseMixerPad Structs *
 ***************************/

typedef struct _GstBaseMixerPad GstBaseMixerPad;
typedef struct _GstBaseMixerPadClass GstBaseMixerPadClass;
typedef struct _GstBaseMixerPadPrivate GstBaseMixerPadPrivate;

/**
 * GstBaseMixerPad:
 * @buffer: currently queued buffer.
 * @segment: last segment received.
 *
 * The implementation of the GstPad to use with #GstBaseMixer
 */
struct _GstBaseMixerPad
{
  GstAggregatorPad            parent;

  GstBuffer                *  buffer;
  GstClockTime                buffer_start_ts;
  GstClockTime                buffer_end_ts;

  /* < Private > */
  GstBaseMixerPadPrivate   *  priv;

  gpointer _gst_reserved[GST_PADDING];
};

/**
 * GstBaseMixerPadClass:
 * @flush:    Optional
 *            Called when the pad has received a flush stop, this is the place
 *            to flush any information specific to the pad, it allows for individual
 *            pads to be flushed while others might not be.
 *
 */
struct _GstBaseMixerPadClass
{
  GstAggregatorPadClass   parent_class;

  GstFlowReturn (*flush)  (GstBaseMixerPad * bmpad, GstBaseMixer * bmixer);

  /*< private >*/
  gpointer      _gst_reserved[GST_PADDING_LARGE];
};

GType gst_base_mixer_pad_get_type           (void);

/****************************
 * GstBaseMixerPad methods *
 ***************************/

gboolean gst_base_mixer_pad_is_eos (GstBaseMixerPad * mixerpad);

/*********************
 * GstBaseMixer API *
 ********************/

#define GST_TYPE_BASE_MIXER            (gst_base_mixer_get_type())
#define GST_BASE_MIXER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_MIXER,GstBaseMixer))
#define GST_BASE_MIXER_CAST(obj)       ((GstBaseMixer *)(obj))
#define GST_BASE_MIXER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_MIXER,GstBaseMixerClass))
#define GST_BASE_MIXER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_BASE_MIXER,GstBaseMixerClass))
#define GST_IS_BASE_MIXER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_MIXER))
#define GST_IS_BASE_MIXER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_MIXER))

#define GST_FLOW_NOT_HANDLED           GST_FLOW_CUSTOM_SUCCESS

/**
 * GstBaseMixer:
 * @srcpad: the base mixer's source pad
 *
 * BaseMixer base class object structure.
 */
struct _GstBaseMixer
{
  GstAggregator            parent;

  GstPad                *  srcpad;

  /*< private >*/
  GstBaseMixerPrivate  *  priv;

  gpointer                 _gst_reserved[GST_PADDING_LARGE];
};

/**
 * GstBaseMixerClass:
 */
struct _GstBaseMixerClass {
  GstAggregatorClass   parent_class;

  GstFlowReturn (*mix) (GstBaseMixer * bmixer, GstClockTime start, GstClockTime end);

  /* Subclass can do extra work before mix is called */
  GstFlowReturn (*prepare) (GstBaseMixer * bmixer, gboolean timeout);

  /* Subclass can adjust the start and end times of the next mix call to their liking.
   * It is only allowed to increase @start if it wants to skip some data and to reduce
   * @end if it wants to mix less data in this turn */
  void (*adjust_times) (GstBaseMixer * bmixer, GstClockTime * start, GstClockTime * end);

  /*< private >*/
  gpointer          _gst_reserved[GST_PADDING_LARGE];
};

/*************************
 * GstBaseMixer methods *
 ************************/

GType gst_base_mixer_get_type(void);

GstFlowReturn gst_base_mixer_finish_buffer (GstBaseMixer * bmixer, GstBuffer * buffer);
gboolean gst_base_mixer_check_eos (GstBaseMixer * bmixer);

G_END_DECLS

#endif /* __GST_BASE_MIXER_H__ */
