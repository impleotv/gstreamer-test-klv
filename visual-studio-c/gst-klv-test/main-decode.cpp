#include <stdlib.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <time.h>
#include <cstdlib>
#include <cstdint> 
#include "windows.h"
#include "io.h"


#include <gst/gst.h>
#include <gst/app/gstappsrc.h>



#ifndef F_OK
#define F_OK 0
#endif

#define funcAddr GetProcAddress

const char* PathToLibrary = "../bin/MisbCoreNativeLib.dll";

std::string PathToLicenseFile;
std::string LicenseKey;

struct PcktBuffer
{
    char* buffer;
    int length;
};


typedef char* (*getNodeInfoFunc)();
typedef bool (*activateFunc)(char*, char*);
typedef char* (*decodeFunc)(char*, int len);
typedef void (*cleanUpFunc)();

HINSTANCE handle;
decodeFunc decode601Pckt;

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData
{
    GstElement* pipeline;
    GstElement* source;
    GstElement* tsDemux;
    GstElement* videoQueue;
    GstElement* dataQueue;
    GstElement* h264parse;
    GstElement* avdec;
    GstElement* videoSink;
    GstElement* dataSink;
} CustomData;

/* Handler for the pad-added signal */
static void pad_added_handler(GstElement* src, GstPad* pad, CustomData* data);

/* The appsink has received a buffer */
static GstFlowReturn new_sample(GstElement* sink, CustomData* data)
{
    GstSample* sample;

    /* Retrieve the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample)
    {
        GstBuffer* gstBuffer = gst_sample_get_buffer(sample);

        if (gstBuffer)
        {
            auto pts = GST_BUFFER_PTS(gstBuffer);
            auto dts = GST_BUFFER_DTS(gstBuffer);

            gsize bufSize = gst_buffer_get_size(gstBuffer);
            g_print("Klv buffer size %ld. PTS %ld   DTS %ld\n", bufSize, pts, dts);

            GstMapInfo map;
            gst_buffer_map(gstBuffer, &map, GST_MAP_READ);

            char* jsonPckt = decode601Pckt((char*)map.data, map.size);
            g_print("%s \n", jsonPckt);

            gst_sample_unref(sample);
            return GST_FLOW_OK;
        }
    }

    return GST_FLOW_ERROR;
}

int main(int argc, char* argv[])
{
    CustomData data;
    GstBus* bus;
    GstMessage* msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    if (argc < 2)
    {
        g_printerr("Please provide a file name.\n");
        return -1;
    }

    // copy first argument to fileName  - this is the file to be played
    std::string SrcFileName = argv[1];

    // copy additional arguments (if exist) to license file path and license key
    if (argc > 3)
    {
        PathToLicenseFile = argv[2];
        LicenseKey = argv[3];
    }

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Check that misbCoreNative library exists */
    if (_access((char*)PathToLibrary, F_OK) == -1)
    {
        g_printerr("Couldn't find library at the specified path");
        return -1;
    }

    /* Load library */
    handle = LoadLibraryA((char*)PathToLibrary);
    if (handle == 0)
    {
        g_printerr("Couldn't load library");
        return -1;
    }

    /* Get function pointer and read node info*/
    getNodeInfoFunc GetNodeInfo = (getNodeInfoFunc)funcAddr(handle, (char*)"GetNodeInfo");
    char* nodeInfo = GetNodeInfo();
    g_print("The NodeInfo: %s \n", nodeInfo);

    /* Get function pointer and activate license */
    activateFunc Activate = (activateFunc)funcAddr(handle, (char*)"Activate");
    bool fValid = Activate((char*)PathToLicenseFile.c_str(), (char*)LicenseKey.c_str());
    g_print("License: %s \n", fValid ? "Valid" : "Invalid. Demo mode.");

    /* Get function pointer to decode method */
    decode601Pckt = (decodeFunc)funcAddr(handle, (char*)"Decode");

    /* Create the elements */
    data.source = gst_element_factory_make("filesrc", "source");
    data.tsDemux = gst_element_factory_make("tsdemux", "demux");
    data.videoQueue = gst_element_factory_make("queue", "videoQueue");
    data.dataQueue = gst_element_factory_make("queue", "dataQueue");
    data.h264parse = gst_element_factory_make("h264parse", "h264parse");
    data.avdec = gst_element_factory_make("avdec_h264", "avdec");
    data.videoSink = gst_element_factory_make("autovideosink", "videoSink");
    data.dataSink = gst_element_factory_make("appsink", "dataSink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new("decode-pipeline");

    if (!data.pipeline || !data.source || !data.tsDemux || !data.videoQueue || !data.dataQueue || !data.h264parse || !data.avdec || !data.videoSink || !data.dataSink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Build the pipeline. Note that we are NOT linking the source at this point. We will do it later. */
    gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.tsDemux, data.videoQueue, data.dataQueue, data.h264parse, data.avdec, data.videoSink, data.dataSink, NULL);

    if (!gst_element_link(data.source, data.tsDemux))   
    {
        g_printerr("Cannot link source to demux.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    if (!gst_element_link_many(data.videoQueue, data.h264parse, data.avdec, data.videoSink, NULL))
    {
        g_printerr("Video processing elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    if (!gst_element_link(data.dataQueue, data.dataSink))
    {
        g_printerr("Data processing elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    /* Set the URI to play */
    g_object_set(G_OBJECT(data.source), "location", SrcFileName.c_str(), NULL);

    /* Configure appsink */
    g_object_set(data.dataSink, "emit-signals", TRUE, NULL);
    g_signal_connect(data.dataSink, "new-sample", G_CALLBACK(new_sample), &data);

    /* Connect to the pad-added signal */
    g_signal_connect(data.tsDemux, "pad-added", G_CALLBACK(pad_added_handler), &data);

    /* Start playing */
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    /* Listen to the bus */
    bus = gst_element_get_bus(data.pipeline);
    do
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

        /* Parse message */
        if (msg != NULL)
        {
            GError* err;
            gchar* debug_info;

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
    cleanUpFunc cleanUp = (cleanUpFunc)funcAddr(handle, (char*)"CleanUp");
    cleanUp();

    return 0;
}

/* This function will be called by the pad-added signal */
static void pad_added_handler(GstElement* src, GstPad* new_pad, CustomData* data)
{
    GstElement* sink;
    GstPad* sink_pad = NULL;
    GstPadLinkReturn ret;
    GstCaps* new_pad_caps = NULL;
    GstStructure* new_pad_struct = NULL;
    const gchar* new_pad_type = NULL;

    /* Check the new pad's type */
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);

    g_print("Received new pad '%s' from '%s' of type '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src), new_pad_type);

    if (g_str_has_prefix(new_pad_type, "video/x-h264"))
        sink_pad = gst_element_get_static_pad(data->videoQueue, "sink");
    else if (g_str_has_prefix(new_pad_type, "meta/x-klv"))
        sink_pad = gst_element_get_static_pad(data->dataQueue, "sink");
    else
    {
        sink = gst_element_factory_make("fakesink", NULL);
        gst_bin_add(GST_BIN(data->pipeline), sink);
        sink_pad = gst_element_get_static_pad(sink, "sink");
        gst_element_sync_state_with_parent(sink);
    }

    if (gst_pad_is_linked(sink_pad) || sink_pad == NULL)
    {
        g_print("We are already linked. Ignoring.\n");
        goto exit;
    }

    /* Attempt the link */
    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret))
        g_print("Type is '%s' but link failed.\n", new_pad_type);
    else
        g_print("Link succeeded (type '%s').\n", new_pad_type);

exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);

    /* Unreference the sink pad */
    if (sink_pad != NULL)
        gst_object_unref(sink_pad);
}