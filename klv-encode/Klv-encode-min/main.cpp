#include <iostream>
#include <cassert>
#include <string>
#include <thread>
#include <stdio.h>
#include <unistd.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <dlfcn.h>
#include <unistd.h>

#define funcAddr dlsym

#ifndef F_OK
#define F_OK 0
#endif

#if defined(_WIN32)
const char *PathToLibrary = "../../../bin/win-x64/MisbCoreNativeLib.dll";
#else
// For x64
const char *PathToLibrary = "../../../bin/linux-x64/MisbCoreNativeLib.so";
// for ARM
// const char *PathToLibrary = "../../bin/linux-arm64/MisbCoreNativeLib.so";
#endif

std::string PathToLicenseFile;
std::string LicenseKey;

struct PcktBuffer
{
	char* buffer;
	int length;
};

typedef char* (*getNodeInfoFunc)();
typedef bool (*activateFunc)(char*, char*);
typedef PcktBuffer (*encodeFunc)(char*);
typedef char* (*decodeFunc)(char*, int len);
typedef void (*cleanUpFunc)();

void *handle;
encodeFunc encode601Pckt;

static GMainLoop *loop;
static std::chrono::steady_clock::time_point lastPcktTime = std::chrono::steady_clock::now();
static int frameRate = 30;
static float frameDuration = 1.0 / (float)frameRate;
static unsigned long long counter = 0;

static void pushKlv(GstElement *src, guint, GstElement);

const char *jsonPcktStr = R"(
      {
        "3": "MISSION01",
        "4": "AF-101",
        "5": 159.974365,
        "6": -0.431531724,
        "7": 3.40586566,
        "8": 147,
        "9": 159,
        "10": "MQ1-B",
        "11": "EO",
        "12": "WGS-84",
        "13": 60.176822966978335,
        "14": 128.42675904204452,
        "15": 14190.7195,
        "16": 144.571298,
        "17": 152.643626,
        "18": 160.71921143697557,
        "19": -168.79232483394085,
        "20": 176.86543764939194,     
        "48": {
        "1": "UNCLASSIFIED//",
        "2": "ISO_3166_ThreeLetter",
        "3": "//IL",
        "4": "Impleo test flight",
        "5": "Test data set",
        "6": "Releasing Instructions Test",
        "7": "Classified By Impleo",
        "8": "Derived From test",
        "9": "Classification Test",
        "10": "20201021",
        "11": "Marking System",
        "12": "ISO_3166_TwoLetter",
        "13": "IL",
        "14": "Comments",
        "22": 9,
        "23": "2020-10-21",
        "24": "2020-10-21"
        },      
        "65": 14       
    })";

int main(int argc, char *argv[])
{
  GstElement *pipeline;
  GstElement *source, *srcCapsFilter, *dataSrc, *encoder264, *mpegtsmux, *parser, *vidCapsFilter, *videoConvert, *videoScale, *timeoverlay, *avsink, *video_queue, *filesink;
  GstCaps *src_filter_caps, *source_caps, *video_filter_caps, *time_filter_caps;
  GstStateChangeReturn ret;
  GstBus *bus;
  GstMessage *msg;
  gboolean terminate = FALSE;

  if (argc < 2)
  {
    g_printerr("Please provide a target file name.\n");
    return -1;
  }

  // copy first argument to fileName  - this is the file we will write to
  std::string TargerPath = argv[1];

  // copy additional arguments (if exist) to license file path and license key
  if (argc > 3)
  {
    PathToLicenseFile = argv[2];
    LicenseKey = argv[3];
  }

  gst_init(&argc, &argv);

  loop = g_main_loop_new(NULL, false);

  source = gst_element_factory_make("videotestsrc", "source");
  srcCapsFilter = gst_element_factory_make("capsfilter", NULL);
  encoder264 = gst_element_factory_make("x264enc", NULL);
  mpegtsmux = gst_element_factory_make("mpegtsmux", NULL);
  video_queue = gst_element_factory_make("queue", "video_queue");
  parser = gst_element_factory_make("h264parse", NULL);
  vidCapsFilter = gst_element_factory_make("capsfilter", NULL);
  videoConvert = gst_element_factory_make("videoconvert", NULL);
  videoScale = gst_element_factory_make("videoscale", NULL);
  timeoverlay = gst_element_factory_make("timeoverlay", NULL);
  dataSrc = gst_element_factory_make("appsrc", NULL);
  avsink = gst_element_factory_make("autovideosink", "sink");
  filesink = gst_element_factory_make("filesink", "filesink");

  if (access((char *)PathToLibrary, F_OK) == -1)
  {
    g_printerr("Couldn't find library at the specified path");
    return -1;
  }

  handle = dlopen((char *)PathToLibrary, RTLD_LAZY);
  if (handle == 0)
  {
    g_printerr("Couldn't load library");
    return -1;
  }

  getNodeInfoFunc GetNodeInfo = (getNodeInfoFunc)funcAddr(handle, (char *)"GetNodeInfo");
  char *nodeInfo = GetNodeInfo();
  g_print("The NodeInfo: %s \n", nodeInfo);

	encode601Pckt = (encodeFunc)funcAddr(handle, (char*)"Encode");

  pipeline = gst_pipeline_new("encode-pipeline");

  if (!pipeline || !source || !srcCapsFilter || !dataSrc || !mpegtsmux || !parser || !vidCapsFilter || !videoConvert || !videoScale || !timeoverlay || !avsink || !video_queue || !filesink)
  {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  g_object_set(G_OBJECT(dataSrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "spare", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
  g_object_set(G_OBJECT(dataSrc), "format", GST_FORMAT_TIME, NULL);
  g_object_set(G_OBJECT(dataSrc), "do-timestamp", TRUE, NULL);

  source_caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 720, "height", G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, frameRate, 1, NULL);
  g_object_set(srcCapsFilter, "caps", source_caps, NULL);

  g_object_set(G_OBJECT(encoder264), "bitrate", 2000000, NULL);

  video_filter_caps = gst_caps_from_string("video/x-h264, stream-format=(string)byte-stream");
  g_object_set(vidCapsFilter, "caps", video_filter_caps, NULL);

  g_object_set(G_OBJECT(filesink), "location", TargerPath.c_str(), NULL);

  /* Assign callback to encode and push metadata */
  g_signal_connect(dataSrc, "need-data", G_CALLBACK(pushKlv), NULL);

  gst_bin_add_many(GST_BIN(pipeline), source, videoConvert, videoScale, timeoverlay, srcCapsFilter, encoder264, vidCapsFilter, parser, mpegtsmux, video_queue, dataSrc, filesink, NULL);
  if (!gst_element_link_many(source, srcCapsFilter, timeoverlay, encoder264, vidCapsFilter, video_queue, mpegtsmux, NULL) ||
      !gst_element_link_many(dataSrc, mpegtsmux, NULL) ||
      !gst_element_link_many(mpegtsmux, filesink, NULL))
  {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  g_object_set(source, "pattern", 0, NULL);

  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_main_loop_run(loop);

  /* Listen to the bus */
  bus = gst_element_get_bus(pipeline);
  do
  {
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    /* Parse message */
    if (msg != NULL)
    {
      GError *err;
      gchar *debug_info;

      switch (GST_MESSAGE_TYPE(msg))
      {
      case GST_MESSAGE_ERROR:
        gst_message_parse_error(msg, &err, &debug_info);
        g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);
        terminate = TRUE;
        break;

      case GST_MESSAGE_STATE_CHANGED:
        /* We are only interested in state-changed messages from the pipeline */
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline))
        {
          GstState old_state, new_state, pending_state;
          gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
          g_print("Pipeline state changed from %s to %s:\n",
                  gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
        }
        break;
      default:
        /* We should not reach here */
        g_printerr("Unexpected message received.\n");
        break;
      }
      gst_message_unref(msg);
    }
  } while (!terminate);

  /* Free resources */
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);

  /* Clean up allocated resources */
  cleanUpFunc cleanUp = (cleanUpFunc)funcAddr(handle, (char *)"CleanUp");
  cleanUp();

  return 0;
}

/* Callback function for encoding and injection of Klv metadata */
static void pushKlv(GstElement *src, guint, GstElement)
{
  GstFlowReturn ret;
  GstBuffer *buffer;
  bool fInsert = false;

  // Wait for a frame duration interval since the last inserted packet. As we're inserting ASYNC KLV, it is good enough for the demo...
  while (!fInsert)
  {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    float diff = std::chrono::duration<float>(now - lastPcktTime).count();

    if (diff > frameDuration)
      fInsert = true;
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // First, encode the klv buffer from jsonPcktStr
  PcktBuffer pcktBuf = encode601Pckt((char*)(jsonPcktStr));

  buffer = gst_buffer_new_allocate(NULL, pcktBuf.length, NULL);

  // For ASYNC_KLV, we need to remove timestamp and duration from the buffer
  GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

  // Fill the buffer with the encoded KLV data
  gst_buffer_fill(buffer, 0, pcktBuf.buffer, pcktBuf.length);

  ret = gst_app_src_push_buffer((GstAppSrc *)src, buffer);

  if (ret != GST_FLOW_OK)
  {
    g_printerr("Flow error");
    g_main_loop_quit(loop);
  }
  else
  {
    lastPcktTime = std::chrono::steady_clock::now();
    counter++;
    g_print("Klv packet count: %llu.  Buf size: %d \n", counter, pcktBuf.length);
  }
}