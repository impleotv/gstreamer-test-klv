# Decoding STANAG 4609 MISB KLV with GStreamer and MisbCoreNative library

This sample application demonstrates how to create a GStreamer pipeline for extracting and decoding MISB601 KLV metadata from STANAG 4609 files/streams using MisbCoreNative library.  

We will be manually creating a pipeline that resembles the following:

```cpp
gst-launch-1.0 filesrc location=file.ts ! tsdemux name=demux demux. ! queue ! h264parse ! 'video/x-h264, stream-format=byte-stream' ! avdec_h264 ! autovideosink demux. ! queue ! 'meta/x-klv' ! appsink
```

For the sake of simplicity, let's assume we have a transport file (container) with H.264 video and one Klv PID.
The demuxer will separate them and expose them through different output ports. This way, different branches will be created in the pipeline, dealing with video and metadata separately - video will play back and the Klv metadata is decoded and sent to the console.


## Building the pipeline

The code is pretty straightforward - we start manually creating a pipeline that consists of the **filesrc** + **tsdemux** plugins and two processing parts:

- video presentation
- metadata decoding and presentation  

This implements a basic GStreamer concept, known as "Dynamic pipelines", where we're building the pipeline "on the fly", as information becomes available.
**tsdemux** will fire events on every elementary stream found in the multiplex, so we could connect the corresponding processing parts.


## Walkthrough

*Below we'll show some essential code snippets - everything else is regular GStreamer code...*  

So, we start by building the pipeline from the source down to the demuxer, and set it to run. 

Before we run it, let's assign a callback:

```cpp
/* Connect to the pad-added signal */
  g_signal_connect(data.tsDemux, "pad-added", G_CALLBACK(pad_added_handler), &data);
```

