#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>

typedef struct SampleHandlerUserData
{
    int pipelineId;
} SampleHandlerUserData;



static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
    GMainLoop *loop = (GMainLoop *)data;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug_info;

            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);

            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }

    return TRUE;
}

static void pushKlv(GstElement *src); 
static GstFlowReturn on_new_sample_from_sink(GstElement *sink, gpointer user_data);


typedef struct {
    unsigned char *buffer;
    size_t length;
} PcktBuffer;

// Global variables
struct timespec lastPcktTime;
unsigned long long counter = 0;
float frameDuration = 0.1; // Set this to the appropriate frame duration
char *jsonPcktStr = "your json string"; // Replace with actual JSON string
GMainLoop *loop;
GstElement *pipeline;

PcktBuffer encode601Pckt(char *jsonPcktStr) {
    PcktBuffer pcktBuf;
    // Encoding logic here
    // For demo purposes, let's assume the encoded buffer is the same as input
    pcktBuf.length = strlen(jsonPcktStr);
    pcktBuf.buffer = (unsigned char *)malloc(pcktBuf.length);
    memcpy(pcktBuf.buffer, jsonPcktStr, pcktBuf.length);
    return pcktBuf;
}


float time_diff(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
}


int main(int argc, char *argv[]) {
    GstElement *videotestsrc, *dataAppSrc, *videoconvert, *video_queue, *x264enc, *h264parse, *rtph264pay, *mpegtsmux, *out_queue, *udpsink, *appsink, *fdsink, *capsfilter, *klvcapsfilter;
    GstBus *bus;
    GstStateChangeReturn ret;
    int sock_fd;
    struct sockaddr_un addr;


    gst_init(&argc, &argv);

    /* Create the elements */
    videotestsrc = gst_element_factory_make("videotestsrc", "videotestsrc");
    dataAppSrc = gst_element_factory_make("appsrc", NULL);
    videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    video_queue = gst_element_factory_make("queue", "video_queue");
    x264enc = gst_element_factory_make("x264enc", "x264enc");
    h264parse = gst_element_factory_make("h264parse", NULL);
    rtph264pay = gst_element_factory_make("rtph264pay", NULL);
    mpegtsmux = gst_element_factory_make("mpegtsmux", "mpegtsmux");
    out_queue = gst_element_factory_make("queue", "out_queue");
//    udpsink = gst_element_factory_make("udpsink", "udpsink");
    capsfilter = gst_element_factory_make("capsfilter", NULL);
    klvcapsfilter = gst_element_factory_make("capsfilter", NULL);
    fdsink = gst_element_factory_make("fdsink", NULL);

// Set capsfilter caps
// GstCaps *caps = gst_caps_new_simple("video/x-h264",
//     "stream-format", G_TYPE_STRING, "byte-stream", NULL);
// g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
// gst_caps_unref(caps);

    // appsink = gst_element_factory_make("appsink", "sink");
    // g_object_set(appsink, "emit-signals", TRUE, NULL);
    // g_object_set(appsink, "async", TRUE, NULL);



    /* Check if elements were created successfully */
    if (!videotestsrc || !videoconvert || !x264enc /*|| !mpegtsmux || !udpsink*/) {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("test-pipeline");

    if (!pipeline) {
        g_printerr("Pipeline could not be created.\n");
        return -1;
    }

    /* Set element properties */
    g_object_set(videotestsrc, "pattern", 0, NULL); /* 0 = Smpte pattern */
    g_object_set(G_OBJECT(videotestsrc), "framerate", GST_TYPE_FRACTION, 30, 1, NULL); // Set framerate to 30 fps
    g_object_set(G_OBJECT(videotestsrc), "pattern", 0, "is-live", TRUE, "do-timestamp", TRUE, "num-buffers", -1, NULL);

    g_object_set(x264enc, "bitrate", 512, "speed-preset", 1, "tune", 4, NULL);
   // g_object_set(udpsink, "host", "127.0.0.1", "port", 1234, "sync", FALSE, "async", TRUE, NULL);
  //  g_object_set(udpsink, "host", "127.0.0.1", "port", 1234, NULL);
    g_object_set (mpegtsmux, "alignment", 7, NULL);
    g_object_set(G_OBJECT(dataAppSrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "sparse", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
    g_object_set(G_OBJECT(dataAppSrc), "format", GST_FORMAT_TIME, NULL);
    g_object_set(G_OBJECT(dataAppSrc), "do-timestamp", TRUE, NULL);
 //   g_object_set(G_OBJECT(dataAppSrc), "min-latency", 1000, NULL);
    //    g_object_set(G_OBJECT(dataAppSrc), "max-time", 1000, NULL);
  //  g_object_set(G_OBJECT(dataAppSrc), "leaky-type", TRUE, NULL);
 //   g_object_set(fdsink, "sync", FALSE, "async", TRUE, NULL);

    // Set capsfilter caps
    GstCaps *caps = gst_caps_new_simple("video/x-h264",
        "stream-format", G_TYPE_STRING, "byte-stream", NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);



    // Set up Unix domain socket
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        g_printerr("Error creating socket\n");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/gstreamer.sock");
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        g_printerr("Error connecting to Unix domain socket\n");
        close(sock_fd);
        return -1;
    }

    // Set properties if needed
    g_object_set(G_OBJECT(fdsink), "fd", sock_fd, NULL);


//    SampleHandlerUserData *s = calloc(1, sizeof(SampleHandlerUserData));

    // Connect the signal handler to the appsink
//    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample_from_sink), s);



 GstCaps *klvCaps = gst_caps_new_simple("meta/x-klv",
                                        "parsed", G_TYPE_BOOLEAN, TRUE,
                                        "sparse", G_TYPE_BOOLEAN, TRUE,
                                        "is-live", G_TYPE_BOOLEAN, TRUE,
                                        NULL);
    g_object_set(klvcapsfilter, "klvCaps", klvCaps, NULL);
    gst_caps_unref(klvCaps);


    /* Assign callback to encode and push metadata */
   //g_signal_connect(dataAppSrc, "need-data", G_CALLBACK(pushKlv), NULL);


    /* Build the pipeline */
    gst_bin_add_many(GST_BIN(pipeline), videotestsrc, dataAppSrc, videoconvert, x264enc, h264parse, /*rtph264pay,*/ capsfilter,/* klvcapsfilter, mpegtsmux, udpsink, appsink,*/ fdsink, NULL);

    if (!gst_element_link_many(videotestsrc, videoconvert, x264enc, h264parse, capsfilter, /* rtph264pay, mpegtsmux, */ fdsink, NULL)) 
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // if (!gst_element_link_many(dataAppSrc, klvcapsfilter, mpegtsmux, NULL)) {
    //     g_printerr("Elements could not be linked.\n");
    //     gst_object_unref(pipeline);
    //     return -1;
    // }


    // if (!gst_element_link_many(mpegtsmux, fdsink, NULL)) {
    //     g_printerr("Elements could not be linked.\n");
    //     gst_object_unref(pipeline);
    //     return -1;
    // }



    /* Start playing the pipeline */
    g_print("Starting pipeline...\n");
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Create a GLib Main Loop and set up the bus to watch for messages */
    loop = g_main_loop_new(NULL, FALSE);

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    /* Run the loop */
    g_print("Running main loop...\n");
    g_main_loop_run(loop);

    /* Free resources */
    g_print("Stopping pipeline...\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}

static GstClockTime count = 0;


void pushKlv(GstElement *src) {
    GstFlowReturn ret;
    GstBuffer *buffer;
    bool fInsert = false;

    while (!fInsert) {
        struct timespec now;
#ifdef CLOCK_MONOTONIC
        clock_gettime(CLOCK_MONOTONIC, &now);
#else
        // Fallback to clock() if CLOCK_MONOTONIC is not available
        clock_t now_clock = clock();
        now.tv_sec = now_clock / CLOCKS_PER_SEC;
        now.tv_nsec = (now_clock % CLOCKS_PER_SEC) * (1000000000 / CLOCKS_PER_SEC);
#endif
        float diff = time_diff(&lastPcktTime, &now);

        if (diff > frameDuration)
            fInsert = true;
        else
            usleep(10000); // sleep for 10 milliseconds
    }

    // First, encode the klv buffer from jsonPcktStr
   // PcktBuffer pcktBuf = encode601Pckt(jsonPcktStr);

   // buffer = gst_buffer_new_allocate(NULL, pcktBuf.length, NULL);


    // Create a GstBuffer of 100 bytes filled with 0x55
    size_t length = 100;
    GstMapInfo map;
    buffer = gst_buffer_new_allocate(NULL, length, NULL);
    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    memset(map.data, 0x55, length);
    gst_buffer_unmap(buffer, &map);

      GST_BUFFER_FLAG_SET(buffer, GST_STREAM_FLAG_SPARSE);

    // For ASYNC_KLV, we need to remove timestamp and duration from the buffer
//    GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
//    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
//    GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;
   GST_BUFFER_PTS(buffer) = 0;
 //  GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
 //  GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

    // GstClockTime time = gst_clock_get_time(gst_system_clock_obtain());
    // GstClockTime base_time = gst_element_get_base_time(pipeline);
    // GstClockTime pts = time - base_time;
    // GST_BUFFER_PTS(buffer) = pts;


    // Fill the buffer with the encoded KLV data
   // gst_buffer_fill(buffer, 0, pcktBuf.buffer, pcktBuf.length);
   gst_buffer_fill(buffer, 0, buffer, length);

    ret = gst_app_src_push_buffer(GST_APP_SRC(src), buffer);

    if (ret != GST_FLOW_OK) {
        g_printerr("Flow error\n");
        g_main_loop_quit(loop);
    } else {
        #ifdef CLOCK_MONOTONIC
        clock_gettime(CLOCK_MONOTONIC, &lastPcktTime);
        #else
        clock_t now_clock = clock();
        lastPcktTime.tv_sec = now_clock / CLOCKS_PER_SEC;
        lastPcktTime.tv_nsec = (now_clock % CLOCKS_PER_SEC) * (1000000000 / CLOCKS_PER_SEC);
        #endif
        counter++;
        // g_print("Klv packet count: %llu.  Buf size: %zu\n", counter, pcktBuf.length);
          g_print("Klv packet count: %llu.  Buf size: %zu\n", counter, length);
    }

    /* Emit the "need-data" signal */
    g_signal_emit_by_name(GST_APP_SRC(src), "need-data", NULL);

    // Free the allocated buffer
  //  free(pcktBuf.buffer);
    free(buffer);
}


static GstFlowReturn on_new_sample_from_sink(GstElement *sink, gpointer user_data)
{
    GstSample *sample;

    gint64 last_pts = -1;
    gint64 last_dts = -1;
    GstBuffer *last_buffer = NULL;

    // Retrieve the sample from the sink
    g_signal_emit_by_name(sink, "pull-sample", &sample, NULL);
    if (sample)
    {
        // Retrieve buffer, PTS, and DTS from the sample
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstClockTime pts = GST_BUFFER_PTS(buffer);
        GstClockTime dts = GST_BUFFER_DTS(buffer);

        // Copy the buffer to a temporary buffer
        if (last_buffer)
            gst_buffer_unref(last_buffer);
        last_buffer = gst_buffer_copy(buffer);

        // Store PTS and DTS in temporary variables
        last_pts = pts;
        last_dts = dts;

        // After processing, you need to release the sample
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}