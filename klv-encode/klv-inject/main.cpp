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

typedef char *(*getNodeInfoFunc)();
typedef bool (*activateFunc)(char *, char *);
typedef char *(*encodeFunc)(char *, int &len);
typedef void (*cleanUpFunc)();

void *handle;
encodeFunc encode601Pckt;

static GMainLoop *loop;
static std::chrono::steady_clock::time_point lastPcktTime = std::chrono::steady_clock::now();
static int frameRate = 30;
static float frameDuration = 1.0 / (float)frameRate;
static unsigned long long counter = 0;

static void pushKlv(GstElement *src, guint, GstElement);

typedef struct _CustomData
{
  GstElement *pipeline;
  GstElement *urisourcebin;
  GstElement *uridecodebin;
  GstElement *parsebin;
  GstElement *tsdemux;
  GstElement *h264parse;
  GstElement *dataSrc;
  GstElement *videoQueue;
  GstElement *dataQueue;
  GstElement *mpegtsmux;
  GstElement *filesink;
  GstElement *fakesink;
  GstElement *appsrc;
//  GstElement *appsink;
} CustomData;

static void pad_added_handler(GstElement *src, GstPad *pad, CustomData *data);
static void pad_unknown_type_handler(GstElement *src, GstPad *pad, CustomData *data);
static GstFlowReturn new_sample_handler(GstElement *object, CustomData *data);
static void autoplug_continue_handler(GstElement *src, GstPad *pad, GstCaps *caps, CustomData *data);
static GstPad * gst_my_filter_request_new_pad(GstElement *element, GstPadTemplate *templ, const gchar *name, const GstCaps  *caps);
static void gst_my_filter_release_pad (GstElement *element, GstPad *pad);



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
  CustomData data;
  GstCaps *src_filter_caps, *source_caps, *video_filter_caps, *time_filter_caps;
  GstStateChangeReturn ret;
  GstBus *bus;
  GstMessage *msg;
  gboolean terminate = FALSE;

  if (argc < 2)
  {
    g_printerr("Please provide a target url.\n");
    return -1;
  }

  // copy first argument to fileName  - this is the file we will write to
  std::string SourcePath = argv[1];
  std::string TargerPath = argv[2];

  // copy additional arguments (if exist) to license file path and license key
  if (argc > 4)
  {
    PathToLicenseFile = argv[3];
    LicenseKey = argv[4];
  }

  gst_init(&argc, &argv);

  loop = g_main_loop_new(NULL, false);

  //   source = gst_element_factory_make("urisourcebin", "source");
  //   parsebin = gst_element_factory_make("parsebin", "parsebin");
  //   decodebin = gst_element_factory_make("decodebin", "decodebin");
  //   tsDemux = gst_element_factory_make("tsdemux", "demux");
  //   h264parser = gst_element_factory_make("h264parse", NULL);
  //   mpegtsmux = gst_element_factory_make("mpegtsmux", NULL);
  //   video_queue = gst_element_factory_make("queue", "video_queue");
  //   parser = gst_element_factory_make("h264parse", NULL);

  //   avsink = gst_element_factory_make("autovideosink", "sink");
  //   udpsink = gst_element_factory_make("udpsink", NULL);

  //data.dataSrc = gst_element_factory_make("appsrc", "appsrc");
  //data.dataQueue = gst_element_factory_make("queue", "dataQueue");
  //data.videoQueue = gst_element_factory_make("queue", "videoQueue");

  // data.appsink = gst_element_factory_make("appsink", "appsink");
  //data.mpegtsmux = gst_element_factory_make("mpegtsmux", "mpegtsmux");
  //data.filesink = gst_element_factory_make("filesink", "filesink");

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

  // data.pipeline = gst_pipeline_new("encode-pipeline");

  // data.pipeline = gst_parse_launch("urisourcebin uri=file:///home/alexc/Movies/2t.ts name=urisourcebin ! parsebin name=parsebin ! fakesink", NULL);
  // data.pipeline = gst_parse_launch("urisourcebin uri=file:///home/alexc/Movies/bipbop.ts name=urisourcebin ! parsebin name=parsebin parsebin. ! queue ! decodebin ! autovideosink parsebin. ! queue ! appsink name=appsink", NULL);
  // data.pipeline = gst_parse_launch("urisourcebin uri=file:///home/alexc/Movies/bipbop.ts name=urisourcebin ! parsebin name=parsebin", NULL);
  // data.pipeline = gst_parse_launch("urisourcebin uri=file:///home/alexc/Movies/bipbop.ts name=urisourcebin ! tee name=t t. ! decodebin ! autovideosink t. ! queue ! tsdemux name=tsdemux" , NULL);
  //data.pipeline = gst_parse_launch("urisourcebin uri=rtsp://192.168.1.3/onvif1 name=urisourcebin ! tee name=t t. ! decodebin ! autovideosink t. ! queue ! tsdemux name=tsdemux" , NULL);
 
  
   const gchar * pipelineStr =  "rtspsrc location=rtsp://192.168.1.3/onvif1 ! decodebin ! timeoverlay halignment=center valignment=bottom ! video/x-raw ! x264enc tune=zerolatency ! tee name=tee "
                   " tee. ! queue ! decodebin ! autovideoconvert ! autovideosink "
                   " tee. ! video/x-h264, stream-format=byte-stream ! mpegtsmux name=mpegtsmux ! filesink name=filesink location=/home/alexc/tmp/test.ts async=false";       

  g_print("%s", pipelineStr);
  data.pipeline = gst_parse_launch(pipelineStr, NULL); 
 
  // data.pipeline = gst_parse_launch("rtspsrc location=rtsp://192.168.1.3/onvif1 ! decodebin ! timeoverlay halignment=center valignment=bottom ! video/x-raw ! x264enc  tune=zerolatency ! tee name=tee  tee. ! queue ! decodebin ! autovideoconvert ! autovideosink  tee. ! video/x-h264, stream-format=byte-stream !  mpegtsmux ! filesink name=filesink async=false", NULL); 
  //  gst-launch-1.0 rtspsrc location=rtsp://192.168.1.3/onvif1 ! decodebin ! timeoverlay halignment=center valignment=bottom ! video/x-raw !  x264enc  tune=zerolatency  ! tee name=tee tee. ! queue ! decodebin ! autovideoconvert ! autovideosink tee. ! video/x-h264, stream-format=byte-stream !  mpegtsmux ! filesink location=/home/alexc/tmp/test.ts 
   //   data.pipeline = gst_parse_launch("urisourcebin uri=file:///home/alexc/Movies/bipbop.ts name=urisourcebin ! tee name=t t. ! decodebin ! autovideosink t. ! decodebin ! autovideosink" , NULL);
  // data.pipeline = gst_parse_launch("uridecodebin name=uridecodebin uri=file:///home/alexc/Movies/2t.ts ! fakesink", NULL);
  // data.pipeline = gst_parse_launch("urisourcebin uri=file:///home/alexc/Movies/2t.ts ! parsebin name=parsebin ! decodebin ! autovideosink", NULL);

 // data.mpegtsmux = gst_element_factory_make("mpegtsmux", "mpegtsmux");
 // data.filesink = gst_element_factory_make("filesink", "filesink");
 // data.h264parse = gst_element_factory_make("h264parse", "h264parse");
  // data.appsink = gst_element_factory_make("appsink", "appsink");


 // GstElement* tsDemuxPrev = gst_element_factory_make("tsdemux", "demuxPreview");
//  GstElement* videoQueue = gst_element_factory_make("queue", "videoQueue");
//  GstElement* tsQueue = gst_element_factory_make("queue", "tsQueue");
 // data.fakesink = gst_element_factory_make("fakesink", "fakesink");
  // GstElement* h264parse = gst_element_factory_make("h264parse", "h264parse");
  // GstElement* avdec = gst_element_factory_make("avdec_h264", "avdec");
  // GstElement* videoSink = gst_element_factory_make("autovideosink", "videoSink");

  GstElement* filesink = gst_bin_get_by_name (GST_BIN(data.pipeline), "filesink");
  g_object_set(G_OBJECT(filesink), "location", TargerPath.c_str(), NULL);


  data.appsrc = gst_element_factory_make("appsrc", "appsrc");
  gst_bin_add(GST_BIN(data.pipeline), data.appsrc);
  g_object_set(G_OBJECT(data.appsrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "spare", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
  g_object_set(G_OBJECT(data.appsrc), "format", GST_FORMAT_TIME, NULL);
  g_object_set(G_OBJECT(data.appsrc), "do-timestamp", TRUE, NULL);
  g_signal_connect(data.appsrc, "need-data", G_CALLBACK(pushKlv), NULL);


  data.mpegtsmux = gst_bin_get_by_name (GST_BIN(data.pipeline), "mpegtsmux");
  
  if(!gst_element_link(data.appsrc, data.mpegtsmux))
  {
    g_print("Failed to link appsrc and mpegtsmux");
    return -1;
  }

  // if (!data.pipeline || !data.mpegtsmux || !data.filesink)
  // {
  //   g_printerr("Not all elements could be created.\n");
  //   return -1;
  // }

 // gst_bin_add_many(GST_BIN(data.pipeline), data.mpegtsmux,  data.fakesink /*data.filesink*/, data.h264parse, NULL);


  // if (!gst_element_link_many( tsDemuxPrev, tsQueue, NULL))
  // {
  //   g_printerr("Elements could not be linked.\n");
  //   gst_object_unref(data.pipeline);
  //   return -1;
  // }


  // if (!gst_element_link_many(data.mpegtsmux, data.fakesink /*data.filesink*/, NULL))
  // {
  //   g_printerr("Elements could not be linked.\n");
  //   gst_object_unref(data.pipeline);
  //   return -1;
  // }

  // g_object_set(G_OBJECT(data.dataSrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "spare", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
  // g_object_set(G_OBJECT(data.dataSrc), "format", GST_FORMAT_TIME, NULL);
  // g_object_set(G_OBJECT(data.dataSrc), "do-timestamp", TRUE, NULL);



  bus = gst_pipeline_get_bus(GST_PIPELINE(data.pipeline));
  g_assert(bus);

  // g_object_set(G_OBJECT(data.dataSrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "spare", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
  // g_object_set(G_OBJECT(data.dataSrc), "format", GST_FORMAT_TIME, NULL);
  // g_object_set(G_OBJECT(data.dataSrc), "do-timestamp", TRUE, NULL);

 //  g_object_set(G_OBJECT(data.fakesink), "async", false, NULL);

  // g_object_set(G_OBJECT(source), "uri", SourcePath.c_str(), NULL);
  // // g_object_set(G_OBJECT(source), "uri", "rtsp://192.168.1.3/onvif1", NULL);
  // //g_object_set(G_OBJECT(source), "location", "rtsp://192.168.1.3/onvif1", NULL);

  // g_object_set(G_OBJECT(udpsink), "host", "227.1.1.1", NULL);
  // g_object_set(G_OBJECT(udpsink), "auto-multicast", true, NULL);
  // g_object_set(G_OBJECT(udpsink), "port", 1234, NULL);

  // g_object_set(G_OBJECT(dataSrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "spare", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
  // g_object_set(G_OBJECT(dataSrc), "format", GST_FORMAT_TIME, NULL);
  // g_object_set(G_OBJECT(dataSrc), "do-timestamp", TRUE, NULL);

  // data.parsebin = gst_bin_get_by_name (GST_BIN(data.pipeline), "parsebin");
  // g_assert(data.parsebin);
  // g_signal_connect(data.parsebin, "autoplug-continue", G_CALLBACK(autoplug_continue_handler), &data);
  // g_signal_connect(data.parsebin, "pad-added", G_CALLBACK(pad_added_handler), &data);
  // g_signal_connect(data.parsebin, "unknown-type", G_CALLBACK(pad_unknown_type_handler), &data);


  //  data.tsdemux = gst_bin_get_by_name (GST_BIN(data.pipeline), "tsdemux");
  //  g_assert(data.tsdemux);
  //  g_signal_connect(data.tsdemux, "pad-added", G_CALLBACK(pad_added_handler), &data);




  // data.appsink = gst_bin_get_by_name(GST_BIN(data.pipeline), "appsink");
  // g_assert(data.appsink);
  // g_object_set(data.appsink, "emit-signals", TRUE, NULL);
  // g_signal_connect(data.appsink, "new-sample", G_CALLBACK(new_sample_handler), &data);


  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(data.pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");

  ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
  {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(data.pipeline);
    return -1;
  }

  g_main_loop_run(loop);

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
      case GST_MESSAGE_EOS:
        g_print("End-Of-Stream reached.\n");
        terminate = TRUE;
        break;

      case GST_MESSAGE_STATE_CHANGED:
        /* We are only interested in state-changed messages from the pipeline */
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline))
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
  gst_element_set_state(data.pipeline, GST_STATE_NULL);
  gst_object_unref(data.pipeline);

  /* Clean up allocated resources */
  cleanUpFunc cleanUp = (cleanUpFunc)funcAddr(handle, (char *)"CleanUp");
  cleanUp();

  return 0;
}


static void autoplug_continue_handler(GstElement *src, GstPad *pad, GstCaps *caps, CustomData *data)
{
  gchar *caps_str = gst_caps_to_string(caps);
  g_print("autoplug_continue_handler: %s\n", caps_str);
  g_free(caps_str);
  //gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
}





/* This function will be called by the pad-added signal */
static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data)
{

  GstPad *sink_pad;
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  /* Check the new pad's type */
  new_pad_caps = gst_pad_get_current_caps(new_pad);
  new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
  new_pad_type = gst_structure_get_name(new_pad_struct);
  g_print("Received new pad '%s' from '%s' of type '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src), new_pad_type);


 // GstElement* queue = gst_element_factory_make("queue", NULL);
//  gst_bin_add(GST_BIN(data->pipeline), queue);


//  GstPad *queue_src_pad = gst_element_get_static_pad(queue, "src");
//  GstPad *queue_sink_pad = gst_element_get_static_pad(queue, "sink");

  GstPad *mux_sink_pad = gst_element_get_request_pad(data->mpegtsmux, "sink_%d");

  //  ret = gst_pad_link(queue_src_pad, mux_sink_pad);
  //   if (GST_PAD_LINK_FAILED(ret))
  //     g_print("Type is '%s' but link failed.\n", GST_PAD_NAME(new_pad));
  //   else
  //     g_print("Link succeeded (type '%s').\n", GST_PAD_NAME(new_pad));

  // if (g_str_has_prefix(new_pad_type, "video/x-h264"))
  // {

  //   GstPad *h264parse_src_pad = gst_element_get_static_pad(data->h264parse, "src");
  //   GstPad *h264parse_sink_pad = gst_element_get_static_pad(data->h264parse, "sink");

  //   ret = gst_pad_link(h264parse_src_pad, mux_sink_pad);
  //   if (GST_PAD_LINK_FAILED(ret))
  //     g_print("Type is '%s' but link failed.\n", h264parse_sink_pad);
  //   else
  //     g_print("Link succeeded (type '%s').\n", h264parse_sink_pad);
  // }
  // else
  // {
    ret = gst_pad_link(new_pad, mux_sink_pad);
    if (GST_PAD_LINK_FAILED(ret))
      g_print("Type is '%s' but link failed.\n", new_pad_type);
    else
      g_print("Link succeeded (type '%s').\n", new_pad_type);
  // }

  gboolean st;
 //  st = gst_element_set_state (queue, GST_STATE_PLAYING);
//    st = gst_element_set_state (data->mpegtsmux, GST_STATE_PLAYING);
 //    st = gst_element_set_state (data->fakesink, GST_STATE_PLAYING);
//   st = gst_element_sync_state_with_parent(data->mpegtsmux);
//   st = gst_element_sync_state_with_parent(data->fakesink);

 //  gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
 // gst_element_set_state(data->pipeline, GST_STATE_PLAYING);

  
 // gst_object_unref(queue_src_pad);
 // gst_object_unref(queue_sink_pad);
 // gst_object_unref(mux_sink_pad);


  //   if (gst_pad_is_linked(sink_pad))
  //   {
  //     g_print("We are already linked. Ignoring.\n");
  //     goto exit;
  //   }

  //   /* Attempt the link */
  //   ret = gst_pad_link(new_pad, sink_pad);
  //   if (GST_PAD_LINK_FAILED(ret))
  //     g_print("Type is '%s' but link failed.\n", new_pad_type);
  //   else
  //     g_print("Link succeeded (type '%s').\n", new_pad_type);

  // exit:
  //   /* Unreference the new pad's caps, if we got them */
  if (new_pad_caps != NULL)
     gst_caps_unref(new_pad_caps);
}

/* This function will be called by the punknown_type signal */
static void pad_unknown_type_handler(GstElement *src, GstPad *new_pad, CustomData *data)
{
  GstPad *sink_pad;
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  new_pad_caps = gst_pad_get_current_caps(new_pad);
  new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
  new_pad_type = gst_structure_get_name(new_pad_struct);
  g_print("Received new pad '%s' from '%s' of type '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src), new_pad_type);

  //   if (g_str_has_prefix(new_pad_type, "meta/x-klv"))
  //      sink_pad = gst_element_get_static_pad(data->dataSink, "sink");

  //   if (gst_pad_is_linked(sink_pad))
  //   {
  //     g_print("We are already linked. Ignoring.\n");
  //     goto exit;
  //   }

  //   /* Attempt the link */
  //   ret = gst_pad_link(new_pad, sink_pad);
  //   if (GST_PAD_LINK_FAILED(ret))
  //     g_print("Type is '%s' but link failed.\n", new_pad_type);
  //   else
  //     g_print("Link succeeded (type '%s').\n", new_pad_type);

  // exit:
  // //   /* Unreference the new pad's caps, if we got them */
  //   if (new_pad_caps != NULL)
  //     gst_caps_unref(new_pad_caps);

  //   /* Unreference the sink pad */
  //     gst_object_unref(sink_pad);
}

static GstFlowReturn new_sample_handler(GstElement *src, CustomData *data)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstSample *sample;

  /* Retrieve the buffer */
  g_signal_emit_by_name(src, "pull-sample", &sample);
  if (sample)
  {

    GstCaps *caps;
    caps = gst_sample_get_caps(sample);
    if (!caps)
    {
      g_print("No caps on sample\n");
    }
    else
    {
      GstStructure *str;
      str = gst_caps_get_structure(caps, 0);
      g_print("Caps: %s\n", gst_structure_get_name(str));
    }

    gst_sample_unref(sample);
  }

  return ret;
}

/* Callback function for encoding and injection of Klv metadata */
static void pushKlv(GstElement *src, guint, GstElement)
{
  GstFlowReturn ret;
  GstBuffer *buffer;
  bool fInsert = false;
  int len;

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
  char *buf = encode601Pckt((char *)(jsonPcktStr), len);

  buffer = gst_buffer_new_allocate(NULL, len, NULL);

  // For ASYNC_KLV, we need to remove timestamp and duration from the buffer
  GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

  // Fill the buffer with the encoded KLV data
  gst_buffer_fill(buffer, 0, buf, len);

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
    g_print("\nKlv packet count: %llu.  Buf size: %d \n", counter, len);
  }
}

/* parse stream url udp, rtsp and return the protocol, ip and port */
static void ParseStreamUrl(std::string url)
{
  // std::string protocol;
  // std::string ip;
  // std::string port;

  // std::size_t found = url.find("://");
  // if (found != std::string::npos)
  // {
  //   protocol = url.substr(0, found);
  //   url.erase(0, found + 3);
  // }

  // found = url.find("/");
  // if (found != std::string::npos)
  // {
  //   ip = url.substr(0, found);
  //   url.erase(0, found + 1);
  // }

  // found = url.find(":");
  // if (found != std::string::npos)
  // {
  //   port = url.substr(0, found);
  //   url.erase(0, found + 1);
  // }

  // if (ip.empty())
  //   ip = url;

  // if (port.empty())
  //   port = "5000";

  // if (protocol.empty())
  //   protocol = "udp";

  // g_print("Protocol: %s, IP: %s, Port: %s\n", protocol.c_str(), ip.c_str(), port.c_str());

  // if (protocol == "udp")
  // {
  //   g_object_set(udpsink, "host", ip.c_str(), "port", port.c_str(), NULL);
  // }
  // else if (protocol == "rtsp")
  // {
  //   g_object_set(rtspsrc, "location", url.c_str(), NULL);
  // }
}