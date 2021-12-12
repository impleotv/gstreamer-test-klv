#include <iostream>
#include <cassert>
#include <string>
#include <thread>
#include <stdio.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <dlfcn.h>
#include <unistd.h>

#define funcAddr dlsym

#ifndef F_OK
#define F_OK 0
#endif

const char *PathToLibrary = "../../../misbCoreNative/MisbCoreNativeLib.so";
const char *PathToLicenseFile = "/home/alexc/Licenses/MisbCoreLegion.lic";
const char *LicenseKey = "D6D237EF-6F41CFAF-7A37BA74-6656A4E5";
const char *TargerPath = "/home/alexc/tmp/gTestFile.ts";

typedef char *(*getNodeInfoFunc)();
typedef bool (*activateFunc)(char *, char *);
typedef char *(*encodeFunc)(char *, int &len);
typedef void (*cleanUpFunc)();

void *handle;
encodeFunc encode601Pckt;

static GMainLoop *loop;
static std::string pcktMsg("Packet ");
static std::string pcktLogString;

const char* jsonPcktStr = R"(
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


static void pushKlv(GstElement *src, guint /*length*/, GstElement /**update*/)
{
  GstFlowReturn ret;
  GstBuffer *buffer;
  static int counter = {0};
  bool isUpdate{false};

  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  while (!isUpdate)
  {
    std::chrono::steady_clock::time_point current = std::chrono::steady_clock::now();
    float diff = std::chrono::duration<float>(current - start).count();

    if (diff > 0.04)
    {
      counter++;
      isUpdate = true;
    }
  }

  int len;


  // First, encode the klv buffer from jsonPcktStr
  char *buf = encode601Pckt((char *)(jsonPcktStr), len);
  g_print("The buff len is %d \n", len);

  buffer = gst_buffer_new_allocate(NULL, len, NULL);

  // For ASYNC_KLV, we need to remove timestamp and duration from the buffer
  GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

  gst_buffer_fill(buffer, 0, buf, len);

  pcktLogString = pcktMsg + std::to_string(counter);
  std::cout << pcktLogString << std::endl;

  ret = gst_app_src_push_buffer((GstAppSrc *)src, buffer);

  if (ret != GST_FLOW_OK)
  {
    g_printerr("flow not ok");
    g_main_loop_quit(loop);
  }
}

int main(int argc, char *argv[])
{
  GstElement *pipeline;
  GstElement *source, *srcCapsFilter, *dataSrc, *encoder264, *mpegtsmux, *parser, *vidCapsFilter, *videoConvert, *videoScale, *avsink, *video_queue, *filesink;
  GstCaps *src_filter_caps, *video_filter_caps;
  GstStateChangeReturn ret;
  guint len;

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

  encode601Pckt = (encodeFunc)funcAddr(handle, (char *)"Encode");

  pipeline = gst_pipeline_new("encode-pipeline");

  if (!pipeline || !source || !srcCapsFilter || !dataSrc || !mpegtsmux || !parser || !vidCapsFilter || !videoConvert || !videoScale || !avsink || !video_queue || !filesink)
  {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  g_object_set(G_OBJECT(dataSrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "spare", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
  g_object_set(G_OBJECT(dataSrc), "format", GST_FORMAT_TIME, NULL);
  g_object_set(G_OBJECT(dataSrc), "do-timestamp", TRUE, NULL);

  GstCaps *source_caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, 720, "height", G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 25, 1, NULL);
  g_object_set(srcCapsFilter, "caps", source_caps, NULL);

  g_object_set(G_OBJECT(encoder264), "bitrate", 2000000, NULL);

  video_filter_caps = gst_caps_from_string("video/x-h264, stream-format=(string)byte-stream");
  g_object_set(vidCapsFilter, "caps", video_filter_caps, NULL);

  g_object_set(G_OBJECT(filesink), "location", TargerPath, NULL);
  
  /* Assign callback to push metadata */
  g_signal_connect(dataSrc, "need-data", G_CALLBACK(pushKlv), NULL);

  gst_bin_add_many(GST_BIN(pipeline), source, videoConvert, videoScale, srcCapsFilter, encoder264, vidCapsFilter, parser, mpegtsmux, video_queue, dataSrc, filesink, NULL);
  if (!gst_element_link_many(source, srcCapsFilter, encoder264, vidCapsFilter, video_queue, mpegtsmux, NULL) ||
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

  if (ret == GST_STATE_CHANGE_FAILURE)
  {
    g_printerr("unable to change piplinr state");
    gst_object_unref(pipeline);
    return -1;
  }

  gst_element_set_state(pipeline, GST_STATE_NULL);
}
