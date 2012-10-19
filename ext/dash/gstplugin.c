#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <gst/gst.h>

#include "gstdashdemux.h"
#include "gstdashsink.h"

static gboolean
dashdemux_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "dashdemux", GST_RANK_PRIMARY,
          GST_TYPE_DASH_DEMUX))
    return FALSE;
  if (!gst_element_register (plugin, "dashsink", GST_RANK_SECONDARY,
          GST_TYPE_DASH_SINK))
    return FALSE;
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dashdemux,
    "DASH plugin",
    dashdemux_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
