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

#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <gst/base/gstbytereader.h>
#include "gsthdsclient.h"
#include "gsthds_debug.h"

#define GST_CAT_DEFAULT gst_hds_demux_debug

GstHdsClient *
gst_hds_client_new (void)
{
  GstHdsClient *client = g_new (GstHdsClient, 1);

  g_queue_init (&client->medias);

  return client;
}

static void
gst_hds_media_free (GstHdsMedia * media)
{
  g_free (media->url);
  g_free (media);
}

static void
gst_hds_media_free_with_udata (GstHdsMedia * media, gpointer udata)
{
  gst_hds_media_free (media);
}

static void
gst_hds_segment_run_free (GstHdsSegmentRun * srun)
{
  g_list_free_full (srun->quality_modifiers, g_free);
  g_free (srun->segments);
  g_free (srun);
}

static void
gst_hds_fragment_run_free (GstHdsFragmentRun * frun)
{
  g_list_free_full (frun->quality_modifiers, g_free);
  g_free (frun->fragments);
  g_free (frun);
}

static void
gst_hds_bootstrap_info_free (GstHdsBootstrapInfo * info)
{
  g_free (info->id);
  g_free (info->profile);
  g_free (info->movie_identifier);
  g_free (info->drm_data);
  g_free (info->metadata);
  g_list_free_full (info->server_urls, g_free);
  g_list_free_full (info->quality_modifiers, g_free);

  g_list_free_full (info->segment_runs,
      (GDestroyNotify) gst_hds_segment_run_free);
  g_list_free_full (info->fragment_runs,
      (GDestroyNotify) gst_hds_fragment_run_free);

  g_free (info);
}

static void
gst_hds_bootstrap_info_free_with_udata (GstHdsBootstrapInfo * info,
    gpointer udata)
{
  gst_hds_bootstrap_info_free (info);
}

void
gst_hds_client_free (GstHdsClient * client)
{
  g_queue_foreach (&client->medias, (GFunc) gst_hds_media_free_with_udata,
      NULL);
  g_queue_foreach (&client->bootstrap_infos,
      (GFunc) gst_hds_bootstrap_info_free_with_udata, NULL);
  g_free (client);
}

static void
gst_hds_client_add_media (GstHdsClient * client, GstHdsMedia * media)
{
  GST_DEBUG ("adding media: %s (%u kbps)", media->url, media->bitrate);
  g_queue_push_tail (&client->medias, media);
}

static void
gst_hds_bootstrap_info_add_segment_run (GstHdsBootstrapInfo * info,
    GstHdsSegmentRun * srun)
{
  info->segment_runs = g_list_append (info->segment_runs, srun);
}

static void
gst_hds_bootstrap_info_add_fragment_run (GstHdsBootstrapInfo * info,
    GstHdsFragmentRun * frun)
{
  info->fragment_runs = g_list_append (info->fragment_runs, frun);
}

static void
gst_hds_client_add_bootstrapInfo (GstHdsClient * client,
    GstHdsBootstrapInfo * info)
{
  GST_DEBUG ("adding bootstrapInfo: %s", info->id);
#ifndef GST_DISABLE_GST_DEBUG
  {
    GList *iter;
    GList *iter2;
    gint i;

    GST_DEBUG ("Server urls:");
    for (iter = info->server_urls; iter; iter = g_list_next (iter)) {
      GST_DEBUG ("  %s", (gchar *) iter->data);
    }

    GST_DEBUG ("Quality modifiers:");
    for (iter = info->quality_modifiers; iter; iter = g_list_next (iter)) {
      GST_DEBUG ("  %s", (gchar *) iter->data);
    }

    GST_DEBUG ("Segment runs:");
    for (iter = info->segment_runs; iter; iter = g_list_next (iter)) {
      GstHdsSegmentRun *srun = iter->data;
      GST_DEBUG ("  Quality modifiers:");
      for (iter2 = srun->quality_modifiers; iter2; iter2 = g_list_next (iter2)) {
        GST_DEBUG ("    %s", (gchar *) iter->data);
      }
      GST_DEBUG ("  Segments:");
      for (i = 0; i < srun->segments_len; i++) {
        GST_DEBUG ("    %d) first:%u count:%u", i, srun->segments[i].first,
            srun->segments[i].count);
      }
    }

    GST_DEBUG ("Fragment runs:");
    for (iter = info->fragment_runs; iter; iter = g_list_next (iter)) {
      GstHdsFragmentRun *frun = iter->data;
      GST_DEBUG ("  Quality modifiers:");
      for (iter2 = frun->quality_modifiers; iter2; iter2 = g_list_next (iter2)) {
        GST_DEBUG ("    %s", (gchar *) iter->data);
      }
      GST_DEBUG ("  Fragments: (timescale: %u)", frun->timescale);
      for (i = 0; i < frun->fragments_len; i++) {
        GST_DEBUG ("    %d) first:%u ts:%" G_GUINT64_FORMAT
            " dur:%u discont:0x%x", i, frun->fragments[i].first,
            frun->fragments[i].timestamp, frun->fragments[i].duration,
            frun->fragments[i].discont);
      }
    }
  }
#endif
  g_queue_push_tail (&client->bootstrap_infos, info);
}

static gboolean
xml_get_prop_string (xmlNode * node, const gchar * prop_name, gchar ** str)
{
  xmlChar *prop_string;
  gboolean ret = FALSE;

  prop_string = xmlGetProp (node, (const xmlChar *) prop_name);
  if (prop_string) {
    *str = (gchar *) prop_string;
    ret = TRUE;
  }
  return ret;
}

static gboolean
xml_get_prop_uint (xmlNode * node, const gchar * prop_name, guint * value)
{
  gchar *string;
  gboolean ret = FALSE;

  if (xml_get_prop_string (node, prop_name, &string)) {
    gchar *endptr;
    *value = g_ascii_strtoull (string, &endptr, 10);
    if (*value == 0 && endptr == string)
      ret = FALSE;
    else
      ret = TRUE;
  }
  return ret;
}

static gboolean
gst_hds_parse_check_box_type_and_size (GstByteReader * reader, guint32 fourcc)
{
  guint64 size;
  guint32 read_fourcc;

  if (gst_byte_reader_get_remaining (reader) < 8)
    return FALSE;


  size = gst_byte_reader_get_uint32_be_unchecked (reader);
  read_fourcc = gst_byte_reader_get_uint32_le_unchecked (reader);
  if (fourcc != read_fourcc)
    return FALSE;
  if (size == 1) {
    if (!gst_byte_reader_get_uint64_be (reader, &size))
      return FALSE;

    /* deduct this extra size for the size */
    size -= 8;
  }

  if (gst_byte_reader_get_remaining (reader) < size - 8)
    return FALSE;

  return TRUE;
}

static gboolean
gst_hds_parse_afrt_box (GstHdsBootstrapInfo * info, GstByteReader * reader)
{
  guint32 flags;
  guint8 entry_count;
  GstHdsFragmentRun *frun;
  gint i;

  if (!gst_hds_parse_check_box_type_and_size (reader, GST_MAKE_FOURCC ('a',
              'f', 'r', 't'))) {
    return FALSE;
  }
  /* skip version */
  gst_byte_reader_skip (reader, 1);

  if (!gst_byte_reader_get_uint24_le (reader, &flags))
    return FALSE;
  if (flags & 0x1) {            /* this is an update */
    GST_FIXME ("AFRT box updates are not supported at the moment");
    return FALSE;
  }

  frun = g_new0 (GstHdsFragmentRun, 1);
  gst_hds_bootstrap_info_add_fragment_run (info, frun);

  if (!gst_byte_reader_get_uint32_be (reader, &frun->timescale))
    return FALSE;

  /* quality urls */
  if (!gst_byte_reader_get_uint8 (reader, &entry_count))
    return FALSE;
  for (i = 0; i < entry_count; i++) {
    gchar *quality;

    if (!gst_byte_reader_dup_string_utf8 (reader, &quality))
      return FALSE;
    frun->quality_modifiers = g_list_prepend (frun->quality_modifiers, quality);
  }
  frun->quality_modifiers = g_list_reverse (frun->quality_modifiers);

  /* fragment run entries */
  if (!gst_byte_reader_get_uint32_be (reader, &frun->fragments_len))
    return FALSE;
  frun->fragments = g_new (GstHdsFragmentRunEntry, frun->fragments_len);
  for (i = 0; i < frun->fragments_len; i++) {
    if (!gst_byte_reader_get_uint32_be (reader, &frun->fragments[i].first))
      return FALSE;
    if (!gst_byte_reader_get_uint64_be (reader, &frun->fragments[i].timestamp))
      return FALSE;
    if (!gst_byte_reader_get_uint32_be (reader, &frun->fragments[i].duration))
      return FALSE;
    if (frun->fragments[i].duration == 0)
      if (!gst_byte_reader_get_uint8 (reader, &frun->fragments[i].discont))
        return FALSE;
  }

  return TRUE;
}

static gboolean
gst_hds_parse_asrt_box (GstHdsBootstrapInfo * info, GstByteReader * reader)
{
  guint32 flags;
  guint8 entry_count;
  GstHdsSegmentRun *srun;
  gint i;

  if (!gst_hds_parse_check_box_type_and_size (reader, GST_MAKE_FOURCC ('a',
              's', 'r', 't'))) {
    return FALSE;
  }
  /* skip version */
  gst_byte_reader_skip (reader, 1);

  if (!gst_byte_reader_get_uint24_le (reader, &flags))
    return FALSE;
  if (flags & 0x1) {            /* this is an update */
    GST_FIXME ("ASRT box updates are not supported at the moment");
    return FALSE;
  }

  srun = g_new0 (GstHdsSegmentRun, 1);
  gst_hds_bootstrap_info_add_segment_run (info, srun);

  /* quality urls */
  if (!gst_byte_reader_get_uint8 (reader, &entry_count))
    return FALSE;
  for (i = 0; i < entry_count; i++) {
    gchar *quality;

    if (!gst_byte_reader_dup_string_utf8 (reader, &quality))
      return FALSE;
    srun->quality_modifiers = g_list_prepend (srun->quality_modifiers, quality);
  }
  srun->quality_modifiers = g_list_reverse (srun->quality_modifiers);

  /* segment run entries */
  if (!gst_byte_reader_get_uint32_be (reader, &srun->segments_len))
    return FALSE;
  srun->segments = g_new (GstHdsSegmentRunEntry, srun->segments_len);
  if (gst_byte_reader_get_remaining (reader) < srun->segments_len * 8)
    return FALSE;
  for (i = 0; i < srun->segments_len; i++) {
    srun->segments[i].first = gst_byte_reader_get_uint32_be_unchecked (reader);
    srun->segments[i].count = gst_byte_reader_get_uint32_be_unchecked (reader);
  }

  return TRUE;
}

static gboolean
gst_hds_parse_abst_box (GstHdsBootstrapInfo * info, guint8 * data, gsize size)
{
  GstByteReader reader;
  guint8 aux;
  gint i;
  guint8 entry_count;

  gst_byte_reader_init (&reader, data, size);

  if (!gst_hds_parse_check_box_type_and_size (&reader, GST_MAKE_FOURCC ('a',
              'b', 's', 't'))) {
    return FALSE;
  }

  /* skip version and flags */
  gst_byte_reader_skip_unchecked (&reader, 4);

  /* skip bootstrap version */
  gst_byte_reader_skip_unchecked (&reader, 4);

  aux = gst_byte_reader_get_uint8_unchecked (&reader);
  info->abst_profile = aux >> 6;
  info->abst_live = aux >> 5 & 1;
  info->abst_update = aux >> 4 & 1;

  info->timescale = gst_byte_reader_get_uint32_be_unchecked (&reader);
  info->current_media_time = gst_byte_reader_get_uint64_be_unchecked (&reader);

  /* skip smpte timecode */
  gst_byte_reader_skip_unchecked (&reader, 8);

  if (!gst_byte_reader_dup_string_utf8 (&reader, &info->movie_identifier))
    return FALSE;


  /* server urls */
  if (!gst_byte_reader_get_uint8 (&reader, &entry_count))
    return FALSE;
  for (i = 0; i < entry_count; i++) {
    gchar *url;

    if (!gst_byte_reader_dup_string_utf8 (&reader, &url))
      return FALSE;
    info->server_urls = g_list_prepend (info->server_urls, url);
  }
  info->server_urls = g_list_reverse (info->server_urls);

  /* quality urls */
  if (!gst_byte_reader_get_uint8 (&reader, &entry_count))
    return FALSE;
  for (i = 0; i < entry_count; i++) {
    gchar *quality;

    if (!gst_byte_reader_dup_string_utf8 (&reader, &quality))
      return FALSE;
    info->quality_modifiers = g_list_prepend (info->quality_modifiers, quality);
  }
  info->quality_modifiers = g_list_reverse (info->quality_modifiers);

  gst_byte_reader_dup_string_utf8 (&reader, &info->drm_data);
  if (info->drm_data && info->drm_data[0] != '\0') {
    GST_FIXME ("DRM is not implemented: '%s'", info->drm_data);
    return FALSE;
  }

  if (!gst_byte_reader_dup_string_utf8 (&reader, &info->metadata))
    return FALSE;

  /* segment run tables */
  if (!gst_byte_reader_get_uint8 (&reader, &entry_count))
    return FALSE;
  for (i = 0; i < entry_count; i++) {
    if (!gst_hds_parse_asrt_box (info, &reader))
      return FALSE;
  }

  /* fragment run tables */
  if (!gst_byte_reader_get_uint8 (&reader, &entry_count))
    return FALSE;
  for (i = 0; i < entry_count; i++) {
    if (!gst_hds_parse_afrt_box (info, &reader))
      return FALSE;
  }

  /* phew... finally */

  return TRUE;
}

static void
gst_hds_client_parse_bootstrapInfo_node (GstHdsClient * client,
    xmlNode * binfo_node)
{
  gchar *url;
  gchar *id = NULL;
  gchar *profile;
  gchar *content;
  guint8 *abst_data;
  gsize abst_len;
  GstHdsBootstrapInfo *info;

  if (xml_get_prop_string (binfo_node, "url", &url)) {
    GST_FIXME ("External bootstrapInfo with 'url' isn't supported,"
        " please file a bug");
    return;
  }

  if (!xml_get_prop_string (binfo_node, "profile", &profile)) {
    GST_WARNING ("Ignoring bootstrapInfo with missing 'profile' attribute");
    return;
  }

  /* optional */
  xml_get_prop_string (binfo_node, "id", &id);

  content = (gchar *) xmlNodeGetContent (binfo_node);
  if (content) {
    abst_data = (guint8 *) g_base64_decode (content, &abst_len);
    xmlFree ((xmlChar *) content);
  } else {
    abst_data = NULL;
    abst_len = 0;
  }

  info = g_new0 (GstHdsBootstrapInfo, 1);
  info->id = id;
  info->profile = profile;

  if (abst_data && !gst_hds_parse_abst_box (info, abst_data, abst_len)) {
    gst_hds_bootstrap_info_free (info);
    GST_WARNING ("Ignoring bootstrapInfo with malformed abst box");
    return;
  }
  g_free (abst_data);

  gst_hds_client_add_bootstrapInfo (client, info);
}

static void
gst_hds_client_parse_media_node (GstHdsClient * client, xmlNode * media_node)
{
  gchar *url;
  guint bitrate;
  GstHdsMedia *media;

  if (!xml_get_prop_uint (media_node, "bitrate", &bitrate)) {
    GST_WARNING ("Ignoring media node, failed to parse bitrate");
    return;
  }
  if (!xml_get_prop_string (media_node, "url", &url)) {
    GST_WARNING ("Ignoring media node, failed to parse url");
    return;
  }

  media = g_new (GstHdsMedia, 1);
  media->url = url;
  media->bitrate = bitrate;
  gst_hds_client_add_media (client, media);
}

static void
gst_hds_client_parse_manifest_node (GstHdsClient * client,
    xmlNode * manifest_node)
{
  xmlNode *cur_node;

  for (cur_node = manifest_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (xmlStrcmp (cur_node->name, (xmlChar *) "media") == 0) {
        gst_hds_client_parse_media_node (client, cur_node);
      } else if (xmlStrcmp (cur_node->name, (xmlChar *) "bootstrapInfo") == 0) {
        gst_hds_client_parse_bootstrapInfo_node (client, cur_node);
      } else {
        GST_DEBUG ("Unparsed manifest node '%s'", (gchar *) cur_node->name);
      }
    }
  }
}

gboolean
gst_hds_client_parse_manifest (GstHdsClient * client, const gchar * data,
    gint size)
{
  xmlDocPtr doc;
  xmlNode *root_element = NULL;
  gboolean ret = TRUE;

  g_return_val_if_fail (data != NULL, FALSE);

  /* this initialize the library and check potential ABI mismatches
   * between the version it was compiled for and the actual shared
   * library used
   */
  LIBXML_TEST_VERSION doc = xmlReadMemory (data, size, "noname.xml", NULL, 0);
  if (doc == NULL) {
    GST_ERROR ("failed to parse the f4m manifest file");
    return FALSE;
  }

  root_element = xmlDocGetRootElement (doc);

  if (root_element->type == XML_ELEMENT_NODE
      && xmlStrcmp (root_element->name, (xmlChar *) "manifest") == 0) {
    gst_hds_client_parse_manifest_node (client, root_element);
  } else {
    GST_ERROR ("This is not an HDS manifest file");
    ret = FALSE;
  }

  xmlFreeDoc (doc);
  return ret;
}
