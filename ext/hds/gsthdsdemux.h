/*
 * HDS demux plugin for GStreamer
 *
 * gsthdsdemux.h
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

#ifndef __GST_HDS_DEMUX_H__
#define __GST_HDS_DEMUX_H__

#include <gst/gst.h>
#include <gst/adaptivedemux/gstadaptivedemux.h>
#include "gsthdsclient.h"

G_BEGIN_DECLS
#define GST_TYPE_HDS_DEMUX \
        (gst_hds_demux_get_type())
#define GST_HDS_DEMUX(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HDS_DEMUX,GstHdsDemux))
#define GST_HDS_DEMUX_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HDS_DEMUX,GstHdsDemuxClass))
#define GST_IS_HDS_DEMUX(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HDS_DEMUX))
#define GST_IS_HDS_DEMUX_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HDS_DEMUX))
#define GST_HDS_DEMUX_CAST(obj) \
	((GstHdsDemux *)obj)

typedef struct _GstHdsDemuxStream GstHdsDemuxStream;
typedef struct _GstHdsDemux GstHdsDemux;
typedef struct _GstHdsDemuxClass GstHdsDemuxClass;

struct _GstHdsDemuxStream
{
  GstAdaptiveDemuxStream parent;
};

struct _GstHdsDemux
{
  GstAdaptiveDemux parent;

  GstHdsClient *client;
};

struct _GstHdsDemuxClass
{
  GstAdaptiveDemuxClass parent_class;
};

GType gst_hds_demux_get_type (void);

G_END_DECLS
#endif /* __GST_HDS_DEMUX_H__ */

