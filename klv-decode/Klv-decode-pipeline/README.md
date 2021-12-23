# Decoding STANAG 4609 MISB KLV with GStreamer and MisbCore library

 This sample application demonstrates how to create **GStreamer** pipeline for extracting and decoding MISB601 KLV metadata from STANAG 4609 files / streams using [MisbCore library](https://www.impleotv.com/content/misbcore/help/index.html)  
 
We will be manually creating a pipeline that resembles the following:

```bash
gst-launch-1.0 filesrc location=file.ts ! tsdemux name=demux demux. ! queue ! h264parse ! 'video/x-h264, stream-format=byte-stream' ! avdec_h264 ! autovideosink demux. ! queue ! 'meta/x-klv' ! appsink

```

> Note, the above pipeline is for code explanation only. It won't work with **gst-launch**!!! 

For the sake of simplicity, lets assume we have a transport file (container) with H.264 video and one Klv PID.
The demuxer will separate them and expose through different output ports. In this way, different branches will be created in the pipeline, dealing with video and metadata separately - video will be played back and the Klv metadata - decoded and sent to the console.

## Building the pipeline

The code is pretty straightforward - we start manually creating a pipeline that consists from the **filesrc** + **tsdemux** plugins and two processing parts:  

- video presentation
- metadata decoding and presentation  

This implements is a basic GStreamer concept, known as "Dynamic pipelines", where we're building the pipeline "on the fly", as information becomes available.  
**tsdemux** will fire events on every elementary stream found in the multiplex, so we could connect the corresponding processing parts.  

### Walkthrough

> Below we'll show some essential code snippets - everything else is regular GStreamer code...

So, we start from building the pipeline from the source down to the demuxer, and set it to run.

Before we run it, lets assign a callback:  

```cpp
/* Connect to the pad-added signal */
  g_signal_connect(data.tsDemux, "pad-added", G_CALLBACK(pad_added_handler), &data);
```

In a callback, we determine the elementary stream types and connect a corresponding processing part:  

```cpp
/* This function will be called by the pad-added signal */
static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data)
{
  ...

  g_print("Received new pad '%s' from '%s' of type '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src), new_pad_type);

  if (g_str_has_prefix(new_pad_type, "video/x-h264"))
    sink_pad = gst_element_get_static_pad(data->videoQueue, "sink");
  else if (g_str_has_prefix(new_pad_type, "meta/x-klv"))
    sink_pad = gst_element_get_static_pad(data->dataQueue, "sink");

  if (gst_pad_is_linked(sink_pad))
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

...

}
```

## Building the processing part

Video processing part is very traditional and not a point of interest here - we parse H.264, decode it and send frames for rendering.

### Metadata processing

#### Metadata extraction

Though creating a special plugin is probably a good idea, we'll be using a standard **appsink** for data processing.  
All we have to do is to configure it to emit events (important!!!) and define a callback:  

```cpp
 /* Configure appsink */
  g_object_set(data.dataSink, "emit-signals", TRUE, NULL);
  g_signal_connect(data.dataSink, "new-sample", G_CALLBACK(new_sample), &data);
```

That's basically it! Now, we'll get a callback with the KLV buffer every time **tsdemux** encounters a KLV metadata in the stream.

#### Metadata decoding

We'll be using **MisbCoreNative** library for KLV and MISB metadata decoding.  

First, we load the library (somewhere at the beginning of application):  

```cpp
void *handle;

/* Load library */
handle = dlopen((char *)PathToLibrary, RTLD_LAZY);

```

Next, we get the pointer to the **decode** method:  


```cpp
typedef char *(*decodeFunc)(char *, int len);
decodeFunc decode601Pckt;

...

 /* Get function pointer to decode method */
decode601Pckt = (decodeFunc)funcAddr(handle, (char *)"Decode");

```

And finally, we can implement the data processing callback:  

```cpp
static GstFlowReturn new_sample(GstElement *sink, CustomData *data)
{
  GstSample *sample;

  /* Retrieve the buffer */
  g_signal_emit_by_name(sink, "pull-sample", &sample);
  if (sample)
  {
    GstBuffer *gstBuffer = gst_sample_get_buffer(sample);

    if (gstBuffer)
    {
      auto pts = GST_BUFFER_PTS(gstBuffer);
      auto dts = GST_BUFFER_DTS(gstBuffer);

      gsize bufSize = gst_buffer_get_size(gstBuffer);
      g_print("Klv buffer size %ld. PTS %ld   DTS %ld\n", bufSize, pts, dts);

      GstMapInfo map;
      gst_buffer_map(gstBuffer, &map, GST_MAP_READ);

      char *jsonPckt = decode601Pckt((char *)map.data, map.size);
      g_print("%s \n", jsonPckt);

      gst_sample_unref(sample);
      return GST_FLOW_OK;
    }
  }

  return GST_FLOW_ERROR;
}
```

In the above code we retrieve the buffer from **GstSample** and send it to **decode601Pckt** function for decoding.  
The return value will be a **json** string with decoded data (according to the **MISB601** standard). We can now parse it to obtain the *key:value* pairs!  
For now, we're simply sending the string to console.  
Additionally to the data we can get **pts** and **dts** timestamps (relevant for **SYNC KLV** only, in case of **ASYNC KLV** they will be **-1**). With this information we can achieve a frame accurate sync to the video.

## Cleaning up

**MisbCore** allocates memory for the last decoded buffer, so we need to free it at the end:  


```cpp
  /* Clean up allocated resources */
	cleanUpFunc cleanUp = (cleanUpFunc)funcAddr(handle, (char*)"CleanUp");
	cleanUp();  
 ``` 

## Source code  

 The complete source code is available as part of SDK. 