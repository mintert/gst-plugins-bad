/*
 * basemixer.c - GstBaseMixer testsuite
 *
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/base/gstbasemixer.h>

/* sum mixer - just sums the values in the buffers */

#define GST_TYPE_TEST_SUM_MIXER            (gst_test_sum_mixer_get_type ())
#define GST_TEST_SUM_MIXER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TEST_SUM_MIXER, GstTestSumMixer))
#define GST_TEST_SUM_MIXER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TEST_SUM_MIXER, GstTestSumMixerClass))
#define GST_TEST_SUM_MIXER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TEST_SUM_MIXER, GstTestSumMixerClass))

#define fail_error_message(msg)     \
  G_STMT_START {        \
    GError *error;        \
    gst_message_parse_error(msg, &error, NULL);       \
    fail_unless(FALSE, "Error Message from %s : %s",      \
    GST_OBJECT_NAME (GST_MESSAGE_SRC(msg)), error->message); \
    g_error_free (error);           \
  } G_STMT_END;

typedef struct _GstTestSumMixer GstTestSumMixer;
typedef struct _GstTestSumMixerClass GstTestSumMixerClass;

static GType gst_test_sum_mixer_get_type (void);

#define BUFFER_DURATION 100000000       /* 10 frames per second */

struct _GstTestSumMixer
{
  GstBaseMixer parent;
};

struct _GstTestSumMixerClass
{
  GstBaseMixerClass parent_class;
};

static GstFlowReturn
gst_test_sum_mixer_mix (GstBaseMixer * bmixer, GstClockTime start,
    GstClockTime end)
{
  GstIterator *iter;
  GstBuffer *buf;
  guint8 sum = 0;
  guint8 *data;
  GstMapInfo mapinfo;
  gboolean all_eos = TRUE;
  gboolean done_iterating = FALSE;

  iter = gst_element_iterate_sink_pads (GST_ELEMENT (bmixer));
  while (!done_iterating) {
    GValue value = { 0, };
    GstAggregatorPad *pad;

    switch (gst_iterator_next (iter, &value)) {
      case GST_ITERATOR_OK:
        pad = g_value_get_object (&value);
        buf = gst_aggregator_pad_get_buffer (pad);

        if (buf) {
          gst_buffer_map (buf, &mapinfo, GST_MAP_READ);
          sum += mapinfo.data[0];
          gst_buffer_unmap (buf, &mapinfo);
          gst_buffer_unref (buf);
        }

        if (gst_aggregator_pad_is_eos (pad) == FALSE)
          all_eos = FALSE;

        g_value_reset (&value);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        GST_WARNING_OBJECT (bmixer, "Sinkpads iteration error");
        done_iterating = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done_iterating = TRUE;
        break;
    }
  }
  gst_iterator_free (iter);

  /* TODO this should be handled by the mixer itself */
  if (all_eos == TRUE) {
    GST_INFO_OBJECT (bmixer, "no data available, must be EOS");
    gst_pad_push_event (GST_AGGREGATOR (bmixer)->srcpad, gst_event_new_eos ());
    return GST_FLOW_EOS;
  }

  data = g_malloc (sizeof (guint8));
  *data = sum;
  buf = gst_buffer_new_wrapped (data, 1);
  GST_BUFFER_PTS (buf) = start;
  GST_BUFFER_DURATION (buf) = end - start;

  gst_base_mixer_finish_buffer (bmixer, buf);

  /* We just check finish_frame return FLOW_OK */
  return GST_FLOW_OK;
}

#define gst_test_sum_mixer_parent_class parent_class
G_DEFINE_TYPE (GstTestSumMixer, gst_test_sum_mixer, GST_TYPE_BASE_MIXER);

static void
gst_test_sum_mixer_class_init (GstTestSumMixerClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseMixerClass *base_mixer_class = (GstBaseMixerClass *) klass;

  static GstStaticPadTemplate _src_template =
      GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY);

  static GstStaticPadTemplate _sink_template =
      GST_STATIC_PAD_TEMPLATE ("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      GST_STATIC_CAPS_ANY);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&_src_template));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&_sink_template));

  gst_element_class_set_static_metadata (gstelement_class, "Mixer",
      "Testing", "Sum N buffers", "Thiago Santos <thiagoss@osg.samsung.com>");

  base_mixer_class->mix = GST_DEBUG_FUNCPTR (gst_test_sum_mixer_mix);
}

static void
gst_test_sum_mixer_init (GstTestSumMixer * self)
{
  GstAggregator *agg = GST_AGGREGATOR (self);
  gst_segment_init (&agg->segment, GST_FORMAT_TIME);
}

static gboolean
gst_test_sum_mixer_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "testsummixer", GST_RANK_NONE,
      GST_TYPE_TEST_SUM_MIXER);
}

static gboolean
gst_test_sum_mixer_plugin_register (void)
{
  return gst_plugin_register_static (GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "testsummixer",
      "Sum buffers",
      gst_test_sum_mixer_plugin_init,
      VERSION, GST_LICENSE, PACKAGE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
}

typedef struct
{
  GList *input_data;
  GstElement *mixer;
  GstPad *sinkpad, *srcpad;

  /*                       ------------------
   * -----------   --------|--              |
   * | srcpad | -- | sinkpad |  basemixer   |
   * -----------   --------|--              |
   *                       ------------------
   *  This is for 1 Chain, we can have several
   */
} ChainData;

typedef struct
{
  GMainLoop *ml;
  GstPad *srcpad,               /* srcpad of the GstBaseMixer */
   *sinkpad;                    /* fake sinkpad to which GstBaseMixer.srcpad is linked */
  GstElement *mixer;
  GList *output;

  /* -----------------|
   * |             ----------    -----------
   * | basemixer   | srcpad | -- | sinkpad |
   * |             ----------    -----------
   * -----------------|
   */
} TestData;

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static gpointer
push_data (gpointer user_data)
{
  ChainData *chain_data = user_data;
  GList *iter;
  GstFlowReturn flow;
  GstCaps *caps;
  GstSegment segment;

  gst_pad_push_event (chain_data->srcpad, gst_event_new_stream_start ("test"));

  caps = gst_caps_new_empty_simple ("foo/x-bar");
  gst_pad_push_event (chain_data->srcpad, gst_event_new_caps (caps));
  gst_caps_unref (caps);

  gst_segment_init (&segment, GST_FORMAT_TIME);
  gst_pad_push_event (chain_data->srcpad, gst_event_new_segment (&segment));

  for (iter = chain_data->input_data; iter; iter = g_list_next (iter)) {
    if (GST_IS_BUFFER (iter->data)) {
      GST_INFO ("Pushing buffer %" GST_PTR_FORMAT " on pad: %s:%s", iter->data,
          GST_DEBUG_PAD_NAME (chain_data->sinkpad));
      flow = gst_pad_push (chain_data->srcpad, (GstBuffer *) iter->data);
      fail_unless (flow == GST_FLOW_OK);
    } else {
      GST_INFO_OBJECT (chain_data->srcpad, "Pushing event: %"
          GST_PTR_FORMAT, iter->data);
      fail_unless (gst_pad_push_event (chain_data->srcpad,
              (GstEvent *) iter->data) == TRUE);
    }
  }
  return NULL;
}

/*
 * Not thread safe, will create a new ChainData which contains
 * an activated src pad linked to a requested sink pad of @bmixer.
 * clear with _chain_data_clear after.
 */
static void
_chain_data_init (ChainData * data, GstElement * mixer, GList * input_data)
{
  static gint num_src_pads = 0;
  gchar *pad_name = g_strdup_printf ("src%d", num_src_pads);

  num_src_pads += 1;
  data->input_data = input_data;

  data->srcpad = gst_pad_new_from_static_template (&srctemplate, pad_name);
  g_free (pad_name);
  gst_pad_set_active (data->srcpad, TRUE);
  data->mixer = mixer;
  data->sinkpad = gst_element_get_request_pad (mixer, "sink_%u");
  fail_unless (GST_IS_PAD (data->sinkpad));
  fail_unless (gst_pad_link (data->srcpad, data->sinkpad) == GST_PAD_LINK_OK);
}

static void
_chain_data_clear (ChainData * data)
{
  /* Just free the list, the refs we had were pushed */
  g_list_free (data->input_data);

  if (data->srcpad)
    gst_object_unref (data->srcpad);
  if (data->sinkpad)
    gst_object_unref (data->sinkpad);
}

static GstFlowReturn
_test_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  TestData *test = g_object_get_qdata (G_OBJECT (pad),
      g_quark_from_static_string ("test-data"));

  test->output = g_list_append (test->output, buffer);
  return GST_FLOW_OK;
}

static gboolean
_test_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  TestData *test = g_object_get_qdata (G_OBJECT (pad),
      g_quark_from_static_string ("test-data"));

  test->output = g_list_append (test->output, event);

  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS)
    g_main_loop_quit (test->ml);
  return TRUE;
}

static void
_test_data_init (TestData * test)
{
  test->mixer = gst_element_factory_make ("testsummixer", NULL);
  test->ml = g_main_loop_new (NULL, TRUE);
  test->srcpad = GST_AGGREGATOR (test->mixer)->srcpad;
  test->sinkpad = gst_pad_new ("sink", GST_PAD_SINK);
  GST_DEBUG ("Srcpad: %p", test->srcpad);

  fail_unless (gst_pad_link (test->srcpad, test->sinkpad) == GST_PAD_LINK_OK);

  g_object_set_qdata (G_OBJECT (test->sinkpad),
      g_quark_from_static_string ("test-data"), test);

  gst_pad_set_chain_function (test->sinkpad, _test_chain);
  gst_pad_set_event_function (test->sinkpad, _test_event);
  gst_pad_set_active (test->sinkpad, TRUE);

  gst_element_set_state (test->mixer, GST_STATE_PLAYING);
}

static void
_test_data_clear (TestData * test)
{
  gst_element_set_state (test->mixer, GST_STATE_NULL);
  gst_object_unref (test->mixer);

  if (test->sinkpad)
    gst_object_unref (test->sinkpad);

  g_list_free_full (test->output, (GDestroyNotify) gst_mini_object_unref);

  g_main_loop_unref (test->ml);
}

static void
compare_event (GstEvent * result, gpointer expected)
{
  fail_unless (GST_IS_EVENT (expected));
  fail_unless (GST_EVENT_TYPE (result) == GST_EVENT_TYPE (expected));
}

static void
compare_buffer (GstBuffer * result, gpointer expected)
{
  GstBuffer *expected_buffer = expected;
  GstMapInfo result_map, expected_map;

  fail_unless (GST_IS_BUFFER (expected));
  fail_unless (GST_BUFFER_PTS (result) == GST_BUFFER_PTS (expected_buffer));
  fail_unless (GST_BUFFER_DURATION (result) ==
      GST_BUFFER_DURATION (expected_buffer));

  gst_buffer_map (result, &result_map, GST_MAP_READ);
  gst_buffer_map (expected_buffer, &expected_map, GST_MAP_READ);
  fail_unless (result_map.size == expected_map.size);
  fail_unless (memcmp (result_map.data, expected_map.data,
          result_map.size) == 0);
  gst_buffer_unmap (expected_buffer, &expected_map);
  gst_buffer_unmap (result, &result_map);
}

static void
compare_output (GList * output, GList * expected_output)
{
  GList *iter, *iter_expected;

  iter_expected = expected_output;

  for (iter = output; iter; iter = g_list_next (iter)) {

    if (GST_IS_EVENT (iter->data)) {
      GstEvent *result = iter->data;
      switch (GST_EVENT_TYPE (result)) {
        case GST_EVENT_STREAM_START:
        case GST_EVENT_CAPS:
        case GST_EVENT_SEGMENT:
          continue;
        case GST_EVENT_EOS:
          compare_event (result, iter_expected->data);
          break;
        default:
          fail ("Unexpected event");
          break;
      }
      iter_expected = g_list_next (iter_expected);
    } else if (GST_IS_BUFFER (iter->data)) {
      compare_buffer (iter->data, iter_expected->data);
      iter_expected = g_list_next (iter_expected);
    }
  }
}

static void
run_test (GList ** inputs, gint n_inputs, GList * expected_output)
{
  gint i;
  GThread **threads;
  ChainData *chains;
  TestData test = { 0, };

  chains = g_malloc0 (sizeof (ChainData) * n_inputs);
  threads = g_malloc0 (sizeof (GThread *) * n_inputs);

  _test_data_init (&test);

  for (i = 0; i < n_inputs; i++) {
    _chain_data_init (&chains[i], test.mixer, inputs[i]);
  }

  for (i = 0; i < n_inputs; i++) {
    threads[i] = g_thread_try_new ("gst-check", push_data, &chains[i], NULL);
  }

  g_main_loop_run (test.ml);

  for (i = 0; i < n_inputs; i++) {
    g_thread_join (threads[i]);
  }

  compare_output (test.output, expected_output);

  for (i = 0; i < n_inputs; i++) {
    _chain_data_clear (&chains[i]);
  }

  _test_data_clear (&test);
  g_list_free_full (expected_output, (GDestroyNotify) gst_mini_object_unref);
  g_free (inputs);
  g_free (chains);
  g_free (threads);
}

static GList *
input_list_from_string (const gchar * str)
{
  GList *list = NULL;
  GstClockTime ts = 0;

  while (*str != '\0') {
    if (*str >= '0' && *str <= '9') {
      GstBuffer *buf;
      guint8 *data = g_malloc (sizeof (guint8));
      data[0] = *str - '0';

      buf = gst_buffer_new_wrapped (data, 1);
      GST_BUFFER_PTS (buf) = ts;
      GST_BUFFER_DURATION (buf) = GST_SECOND;
      list = g_list_append (list, buf);

      ts += GST_SECOND;
    } else {
      fail ("Invalid input string char: 0x%x %c", *str, *str);
    }

    str++;
  }

  return g_list_append (list, gst_event_new_eos ());
}

/* Generates test cases from strings of numbers. All buffers
 * have 1s duration. */
static void
run_test_from_strings (const gchar ** input_strings, gint n_inputs,
    const gchar * expected_output_string)
{
  GList **inputs = g_malloc0 (sizeof (GList *) * n_inputs);
  GList *expected_output = NULL;
  gint i;

  for (i = 0; i < n_inputs; i++) {
    inputs[i] = input_list_from_string (input_strings[i]);
  }

  expected_output = input_list_from_string (expected_output_string);

  run_test (inputs, n_inputs, expected_output);
}

GST_START_TEST (test_mix_1_stream)
{
  const gchar *inputs[] = {
    "111"
  };
  const gchar *output = "111";

  run_test_from_strings (inputs, 1, output);
}

GST_END_TEST;

GST_START_TEST (test_mix_2_streams_aligned_input)
{
  const gchar *inputs[] = {
    "111",
    "222"
  };
  const gchar *output = "333";

  run_test_from_strings (inputs, 2, output);
}

GST_END_TEST;

GST_START_TEST (test_mix_3_streams_aligned_input)
{
  const gchar *inputs[] = {
    "111",
    "222",
    "444"
  };
  const gchar *output = "777";

  run_test_from_strings (inputs, 3, output);
}

GST_END_TEST;

GST_START_TEST (test_mix_streams_aligned_input_finishing_early)
{
  const gchar *inputs[] = {
    "111",
    "22"
  };
  const gchar *output = "331";

  run_test_from_strings (inputs, 2, output);
}

GST_END_TEST;


static Suite *
gst_base_mixer_suite (void)
{
  Suite *suite;
  TCase *general;

  gst_test_sum_mixer_plugin_register ();

  suite = suite_create ("GstBaseMixer");

  general = tcase_create ("general");
  suite_add_tcase (suite, general);
  tcase_add_test (general, test_mix_1_stream);
  tcase_add_test (general, test_mix_2_streams_aligned_input);
  tcase_add_test (general, test_mix_3_streams_aligned_input);
  tcase_add_test (general, test_mix_streams_aligned_input_finishing_early);

  return suite;
}

GST_CHECK_MAIN (gst_base_mixer);
