#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <stdlib.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <time.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sstream>
#include <cstdlib>
#include <nlohmann/json.hpp>

#include <dlfcn.h>
#include <unistd.h>
#define funcAddr dlsym

#ifndef F_OK
#define F_OK 0
#endif

const char *PathToLibrary = "../../misbCoreNative/MisbCoreNativeLib.so";
const char *PathToLicenseFile = "/home/alexc/Licenses/MisbCoreLegion.lic";
const char *LicenseKey = "D6D237EF-6F41CFAF-7A37BA74-6656A4E5";

typedef char *(*getNodeInfoFunc)();
typedef bool (*activateFunc)(char *, char *);
typedef char *(*encodeFunc)(char *, int &len);
typedef void (*cleanUpFunc)();

void *handle;

// Add ${Boost_PROGRAM_OPTIONS_LIBRARY} to cmake
// using namespace std::chrono;
// using namespace boost::posix_time;
using json = nlohmann::json;

static GMainLoop *loop;
static std::string pcktMsg("Packet ");
static std::string pcktLogString;

static bool fASYNC_KLV = true;

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

  auto jspnPckt = json::parse(R"(
      {
        "2": "2021-12-16T13:44:54",
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
        "21": 68590.983298744770,
        "22": 722.819867,
        "23": -10.542388633146132,
        "24": 29.157890122923014,
        "25": 3216.03723,
        "26": 0.0136602540,
        "27": 0.0036602540,
        "28": -0.0036602540,
        "29": 0.0136602540,
        "30": -0.0110621778,
        "31": -0.0051602540,
        "32": 0.0010621778,
        "33": -0.0121602540,
        "34": 2,
        "35": 235.924010,
        "36": 69.8039216,
        "37": 3725.18502,
        "38": 14818.6770,
        "39": 84,
        "40": -79.163850051892850,
        "41": 166.40081296041646,
        "42": 18389.0471,
        "43": 6,
        "44": 30,
        "45": 425.215152,
        "46": 608.9231,
        "47": 49,
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
        "49": 1191.95850,
        "50": -8.67030854,
        "51": -61.8878750,
        "52": -5.08255257,
        "53": 2088.96010,
        "54": 8306.80552,
        "55": 50.5882353,
        "56": 140,
        "57": 3506979.0316063400,
        "58": 6420.53864,
        "59": "TOP GUN",
        "60": 45016,
        "61": 186,
        "62": 1743,
        "63": 2,
        "64": 311.868162,
        "65": 13,
        "67": -86.041207348947040,
        "68": 0.15552755452484243,
        "69": 9.44533455,
        "70": "APACHE",
        "71": 32.6024262,
        "72": "1995-04-16T13:44:54"
    })");

  // Set current time
  //ptime t = microsec_clock::universal_time();
  //jspnPckt["2"] = to_iso_extended_string(t);

  int len;
  encodeFunc encode601Pckt = (encodeFunc)funcAddr(handle, (char *)"Encode");

  auto timeStr = jspnPckt.dump();
  const char *pPcktStr = const_cast<char *>(timeStr.c_str());

  // First, encode the klv buffer from jspnPckt
  char *buf = encode601Pckt((char *)(pPcktStr), len);
  printf("The buff len is %d \n", len);

  buffer = gst_buffer_new_allocate(NULL, len, NULL);

  if (fASYNC_KLV)
  {
    GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;
  }

  gst_buffer_fill(buffer, 0, buf, len);

  pcktLogString = pcktMsg + std::to_string(counter);
  std::cout << pcktLogString << std::endl;

  ret = gst_app_src_push_buffer((GstAppSrc *)src, buffer);

  if (ret != GST_FLOW_OK)
  {
    std::cout << "flow not ok" << std::endl;
    g_main_loop_quit(loop);
  }
}

int main(int argc, char *argv[])
{
  GstElement *pipeline;
  GstElement *srcCapsFilter, *videoConvert, *videoScale;
  GstElement *src, *mpegtsmux, *parser, *capsFilter, *bufferSize, *udpSink, *fileSink, *multifilesink, *avsink, *source, *video_queue, *filesink;
  GstCaps *src_filter_caps, *filter_caps;
  GstStateChangeReturn ret;

  guint len;
  GstElement *dataSrc;
  GstElement *encoder264;
  gst_init(&argc, &argv);

  loop = g_main_loop_new(NULL, false);

  //  src = gst_element_factory_make("v4l2src", NULL);
  source = gst_element_factory_make("videotestsrc", "source");

  srcCapsFilter = gst_element_factory_make("capsfilter", NULL);
  encoder264 = gst_element_factory_make("x264enc", NULL);
  mpegtsmux = gst_element_factory_make("mpegtsmux", NULL);
  video_queue = gst_element_factory_make("queue", "video_queue");
  parser = gst_element_factory_make("h264parse", NULL);
  capsFilter = gst_element_factory_make("capsfilter", NULL);
  bufferSize = gst_element_factory_make("rndbuffersize", NULL);
  udpSink = gst_element_factory_make("udpsink", NULL);
  // multifilesink = gst_element_factory_make("multifilesink"  , NULL);
  videoConvert = gst_element_factory_make("videoconvert", NULL);
  videoScale = gst_element_factory_make("videoscale", NULL);
  dataSrc = gst_element_factory_make("appsrc", NULL);
  avsink = gst_element_factory_make("autovideosink", "sink");
  filesink = gst_element_factory_make("filesink", "filesink");

  assert(dataSrc != NULL);

  if (access((char *)PathToLibrary, F_OK) == -1)
  {
    puts("Couldn't find library at the specified path");
    return -1;
  }

  handle = dlopen((char *)PathToLibrary, RTLD_LAZY);
  if (handle == 0)
  {
    puts("Couldn't load library");
    return -1;
  }

  getNodeInfoFunc GetNodeInfo = (getNodeInfoFunc)funcAddr(handle, (char *)"GetNodeInfo");
  char *nodeInfo = GetNodeInfo();
  printf("The NodeInfo: %s \n", nodeInfo);

  pipeline = gst_pipeline_new("u");

  if (!pipeline || !source || !avsink || !video_queue)
  {
    g_printerr("Not all elements could be created.\n");
    return -1;
  }

  g_object_set(G_OBJECT(dataSrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "spare", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
  g_object_set(G_OBJECT(dataSrc), "format", GST_FORMAT_TIME, NULL);
  g_object_set(G_OBJECT(dataSrc), "do-timestamp", TRUE, NULL);

  GstCaps *source_caps = gst_caps_new_simple("video/x-raw",
                                             "width", G_TYPE_INT, 720,
                                             "height", G_TYPE_INT, 480,
                                             "framerate", GST_TYPE_FRACTION, 25, 1, NULL);

  //g_object_set(G_OBJECT(src), "device", "/dev/video0" , NULL);
  g_object_set(srcCapsFilter, "caps", source_caps, NULL);

  g_object_set(G_OBJECT(bufferSize), "min", 1316, NULL);
  g_object_set(G_OBJECT(bufferSize), "max", 1316, NULL);

  assert(encoder264 != NULL);
  g_object_set(G_OBJECT(encoder264), "bitrate", 2000000, NULL);
  //   g_object_set(G_OBJECT(encoder264), "intra-refresh", true, NULL);

  filter_caps = gst_caps_from_string("video/x-h264, stream-format=(string)byte-stream");
  g_object_set(capsFilter, "caps", filter_caps, NULL);

  g_object_set(G_OBJECT(filesink), "location", "/home/alexc/tmp/gfile.ts", NULL);

  g_object_set(G_OBJECT(udpSink), "host", "227.1.1.1", NULL);
  g_object_set(G_OBJECT(udpSink), "auto-multicast", true, NULL);

  g_object_set(G_OBJECT(udpSink), "port", 30120, NULL);
  // g_object_set(G_OBJECT(udpSink), "async" , false , NULL);
  // g_object_set(G_OBJECT(udpSink), "sync" , false, NULL);
  // g_object_set(G_OBJECT(udpSink), "ttl-mc" , 255, NULL);
  // g_object_set(G_OBJECT(udpSink), "qos-dscp", 30, NULL);

  // g_object_set(G_OBJECT(multifilesink), "next-file", 4, NULL);
  // g_object_set(G_OBJECT(multifilesink), "location", "video%d.ts", NULL);
  // g_object_set(G_OBJECT(multifilesink), "max-file-size" , 10000000, NULL);

  if (!srcCapsFilter)
  {
    std::cout << "srcCapsfilter is NULL" << std::endl;
  }

  g_signal_connect(dataSrc, "need-data", G_CALLBACK(pushKlv), NULL);

  // gst_bin_add_many(GST_BIN(pipeTest), /*src*/source, videoConvert, videoScale, /*srcCapsFilter,*/ encoder264, /* capsFilter,*/ parser, mpegtsmux,
  //                  /*dataSrc, bufferSize,*/ /*multifilesink,*/ avsink /*udpSink*/,  NULL);

  gst_bin_add_many(GST_BIN(pipeline), source, videoConvert, videoScale, srcCapsFilter, encoder264, capsFilter, parser, mpegtsmux, bufferSize, video_queue, dataSrc, filesink, /*udpSink,*/ NULL);
  if (!gst_element_link_many(source, srcCapsFilter, encoder264, capsFilter, video_queue, mpegtsmux, NULL) ||
      !gst_element_link_many(dataSrc, mpegtsmux, NULL) ||
      !gst_element_link_many(mpegtsmux, bufferSize, filesink, /*udpSink,*/ NULL))
  {
    g_printerr("Elements could not be linked.\n");
    gst_object_unref(pipeline);
    return -1;
  }

  // if(!gst_element_link_many(/*src*/source,  encoder264, /* capsFilter,*/ parser, mpegtsmux ,NULL) ||
  //  //   !gst_element_link_many(dataSrc, mpegtsmux, NULL)                            ||
  //     !gst_element_link_many(mpegtsmux, /*bufferSize,*/ avsink, /*udpSink,*/ /*multifilesink,*/ NULL))
  // {
  //     g_printerr("Elements could not be linked");
  // }

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