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

#ifndef __GST_URI_SRC__
#define __GST_URI_SRC__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_URI_SRC \
  (gst_uri_src_get_type())
#define GST_URI_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_URI_SRC,GstUriSrc))
#define GST_URI_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_URI_SRC,GstUriSrcClass))
#define GST_IS_URI_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_URI_SRC))
#define GST_IS_URI_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_URI_SRC))
#define GST_URI_SRC_CAST(obj) ((GstUriSrc *) obj)

typedef struct _GstUriSrc GstUriSrc;
typedef struct _GstUriSrcClass GstUriSrcClass;

struct _GstUriSrc
{
  GstBin parent;

  GstPad *srcpad;

  GstElement *src;

  /* <private> */
  gchar *uri;
};

struct _GstUriSrcClass
{
  GstBinClass parent_class;
};

GType gst_uri_src_get_type (void);

G_END_DECLS

#endif /* __GST_URI_SRC__ */
