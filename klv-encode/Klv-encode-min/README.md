# Encoding STANAG 4609 MISB KLV with GStreamer and MisbCore library

 This sample application demonstrates how to create a simplified **GStreamer** pipeline for encoding and injecting MISB601 KLV metadata into STANAG 4609 files / streams using [MisbCore library](https://www.impleotv.com/content/misbcore/help/index.html)  
 

For the sake of simplicity, we'll use GStreamer's **videotestsrc** as video source, encode it into H.264, encode a static metadata packet (updated with a current time ), multiplex everything and record into standard compliant STANAG 4609 file.

## Building the pipeline

The code is pretty straightforward - we start manually creating a pipeline that consists from the **videotestsrc** and **x264enc** plugins, add multiplexer and a **filesink**. Some additional auxiliary plugins will be added as well...  


### Walkthrough

> Below we'll show some essential code snippets - everything else is a regular GStreamer code...

As with all GStreamer applications, we start from creating a pipeline and loading plugins.  
We'll use **appsrc** as our data source, so let's configure it:  

```cpp
g_object_set(G_OBJECT(dataSrc), "caps", gst_caps_new_simple("meta/x-klv", "parsed", G_TYPE_BOOLEAN, TRUE, "spare", G_TYPE_BOOLEAN, TRUE, "is-live", G_TYPE_BOOLEAN, TRUE, NULL), NULL);
  g_object_set(G_OBJECT(dataSrc), "format", GST_FORMAT_TIME, NULL);
  g_object_set(G_OBJECT(dataSrc), "do-timestamp", TRUE, NULL);
```

We also configure video encoding and file target parameters, but its not really relevant here.

Lets assign a callback **need-data** which will signal that the source needs more data. In this callback we'll encode our metadata and push the RAW KLV:   

```cpp
/* Assign callback to push metadata */
  g_signal_connect(dataSrc, "need-data", G_CALLBACK(pushKlv), NULL);
```

Before we implement the data injection, we need to load the **misbCore** library:  

First, we load the library (somewhere at the beginning of our application):  

```cpp
void *handle;

/* Load library */
handle = dlopen((char *)PathToLibrary, RTLD_LAZY);
```

Next, we get the pointer to the **encode** method:  


```cpp
typedef char *(*encodeFunc)(char *, int &len);
encodeFunc encode601Pckt = (encodeFunc)funcAddr(handle, (char *)"Encode");
...

Now, lets look at the callback. 

```cpp
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
