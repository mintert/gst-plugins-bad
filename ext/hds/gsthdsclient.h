/*
 * HDS manifest client and parsing library
 *
 * gsthdsclient.h
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

#ifndef __GST_HDS_MANIFEST_H__
#define __GST_HDS_MANIFEST_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstHdsClient           GstHdsClient;
typedef struct _GstHdsStream           GstHdsStream;
typedef struct _GstHdsMedia            GstHdsMedia;
typedef struct _GstHdsBootstrapInfo    GstHdsBootstrapInfo;
typedef struct _GstHdsSegmentRun       GstHdsSegmentRun;
typedef struct _GstHdsSegmentRunEntry  GstHdsSegmentRunEntry;
typedef struct _GstHdsFragmentRun      GstHdsFragmentRun;
typedef struct _GstHdsFragmentRunEntry GstHdsFragmentRunEntry;

struct _GstHdsFragmentRunEntry
{
  guint32 first;
  guint64 timestamp;
  guint32 duration;
  guint8 discont;
};

struct _GstHdsFragmentRun {
  guint32 timescale;
  GList *quality_modifiers;
  GstHdsFragmentRunEntry *fragments;
  guint32 fragments_len;
};

struct _GstHdsSegmentRunEntry
{
  guint32 first;
  guint32 count;
};

struct _GstHdsSegmentRun {
  GList *quality_modifiers;
  GstHdsSegmentRunEntry *segments;
  guint32 segments_len;
};

struct _GstHdsBootstrapInfo
{
  gchar *id;
  gchar *profile;

  /* from the abst box */
  guint8   abst_profile;
  gboolean abst_live;
  gboolean abst_update;
  guint32  timescale;
  guint64  current_media_time;
  gchar *  movie_identifier;
  GList *  server_urls;
  GList *  quality_modifiers;
  gchar *  drm_data;
  gchar *  metadata;

  GList *  segment_runs;
  GList *  fragment_runs;
};

struct _GstHdsMedia
{
  gchar *url;
  guint bitrate;
};

struct _GstHdsStream
{
};

struct _GstHdsClient
{
  GQueue medias;
  GQueue bootstrap_infos;
};

/* Basic initialization/deinitialization functions */
GstHdsClient *gst_hds_client_new (void);
void gst_hds_client_free (GstHdsClient * client);

/* MPD file parsing */
gboolean gst_hds_client_parse_manifest (GstHdsClient *client, const gchar *data, gint size);

G_END_DECLS

#endif /* __GST_HDS_MANIFEST_H__ */

