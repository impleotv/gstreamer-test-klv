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

We should also configure video encoding and file target parameters (check out the sample source code for more info).

Lets assign a callback **need-data** which will signal that the source needs more data. In this callback we'll encode our metadata and push the RAW KLV:   

```cpp
/* Assign callback to encode and push metadata */
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
struct PcktBuffer
{
	char* buffer;
	int length;
};

typedef PcktBuffer (*encodeFunc)(char*);
encodeFunc encode601Pckt = (encodeFunc)funcAddr(handle, (char *)"Encode");
...

Now, lets look at the callback. 

```cpp
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
```

### Klv timestamp  

In the sample, we don't provide a mandatory **Tag 2** (timestamp).  
When no timestamp is found, **MisbCore** will automatically set the current time. 


## Running the application.

Now, everything is ready and we can run the application:  

```bash
The NodeInfo: NI-MISBCORE-WCF9EAWJF46EC2EPQQXTFFDEH2NCB0KYXS1NBNHNSE0SB9K2KJAG: 
Klv packet count: 1.  Buf size: 106 
Klv packet count: 2.  Buf size: 106 
Klv packet count: 3.  Buf size: 106 
Klv packet count: 4.  Buf size: 106 
Klv packet count: 5.  Buf size: 106 
Klv packet count: 6.  Buf size: 106 
Klv packet count: 7.  Buf size: 106 
Klv packet count: 8.  Buf size: 106 
Klv packet count: 9.  Buf size: 106 
```
STANAG 4609 file should be created and a packet counter with the buffer size will be sent to console.

## Cleaning up

**MisbCore** allocates memory for the last encoded buffer, so we need to free it at the end:  


```cpp
  /* Clean up allocated resources */
	cleanUpFunc cleanUp = (cleanUpFunc)funcAddr(handle, (char*)"CleanUp");
	cleanUp();  
 ``` 

## Source code  

 The complete source code is available as part of SDK. 