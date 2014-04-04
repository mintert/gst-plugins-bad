/* GStreamer
 * Copyright (C) 2014 Thiago Santos <ts.santos@sisa.samsung.com>
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

#ifndef _GST_TESSERACT_OCR_H_
#define _GST_TESSERACT_OCR_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <tesseract/baseapi.h>

G_BEGIN_DECLS

#define GST_TYPE_TESSERACT_OCR   (gst_tesseract_ocr_get_type())
#define GST_TESSERACT_OCR(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TESSERACT_OCR,GstTesseractOcr))
#define GST_TESSERACT_OCR_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TESSERACT_OCR,GstTesseractOcrClass))
#define GST_IS_TESSERACT_OCR(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TESSERACT_OCR))
#define GST_IS_TESSERACT_OCR_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TESSERACT_OCR))

typedef struct _GstTesseractOcr GstTesseractOcr;
typedef struct _GstTesseractOcrClass GstTesseractOcrClass;

struct _GstTesseractOcr
{
  GstElement base_tesseractocr;

  tesseract::TessBaseAPI *api;

  GstVideoInfo vinfo;

  GstPad *sinkpad;
  GstPad *srcpad;

  GstEvent *pending_segment;
};

struct _GstTesseractOcrClass
{
  GstElementClass base_tesseractocr_class;
};

GType gst_tesseract_ocr_get_type (void);

G_END_DECLS

#endif
