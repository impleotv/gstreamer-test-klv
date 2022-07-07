
# MisbCoreNative with GStreamer

## Introduction


<div align="center">
	<font color="blue">STANAG 4609 / MISB demo applications.</font>
	<p></p>
</div>

**MisbCoreNative** can be used with C/C++ **GStreamer** applications.  
We provide some simplified code samples that are intended to demonstrate how to use the library to encode or decode **MISB KLV** data using GStreamer. 

## Demo apps

Here are some demo apps to help you get started:

### General
[misbcore-test](VisualStudio/vs-demo-apps.md) - basic C++ console app that demonstrates how to load **MisbCoreNative** library and perform encoding/decoding MISB601 metadata.

### GStreamer
<div align="left" margin-bottom= 25px;>
	<font color="red">Please note, out-of-the box, GStreamer only supports ASYNC KLV. With SYNC KLV these demos won't work properly (playback will freeze, etc). You'll need to patch demux and mux to work with the SYNC KLV</font> 
	<p></p>
</div>  

[GStreamer decode](./DemoApps/gst-klv-decode-pipeline.md) - a sample that demonstrates how to create a **GStreamer** pipeline for extracting and decoding **MISB601 KLV** metadata from **STANAG 4609** files/streams using the **MisbCoreNative** library.  

[GStreamer decode](./DemoApps/gst-klv-encode-pipeline.md) - a sample that demonstrates how to create a simplified **GStreamer** pipeline for encoding and injecting **MISB601** KLV metadata into **STANAG 4609** files/streams using **MisbCoreNative** library.  