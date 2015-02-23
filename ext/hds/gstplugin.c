#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <gst/gst.h>

#include "gsthdsdemux.h"

static gboolean
hds_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "hdsdemux", GST_RANK_PRIMARY,
          GST_TYPE_HDS_DEMUX) || FALSE)
    return FALSE;
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    hdsdemux,
    "HDS demuxer plugin",
    hds_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
