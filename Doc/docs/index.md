
# MisbCoreNative with GStreamer

## Introduction


<div align="center">
	<font color="blue">STANAG 4609 / MISB demo applications.</font>
</div>

**MisbCoreNative** can be used with C/C++ **GStreamer** applications.  
We provide some simplified code samples that are intended to demonstrate how to use the library to encode or decode **MISB KLV** data using GStreamer. 

## Demo apps

[misbcore-test](VisualStudio/vs-demo-apps.md) - basic C++ console app that demonstrates how to load **MisbCoreNative** library and perform encoding/decoding MISB601 metadata.


[GStreamer decode](./DemoApps/gst-klv-decode-pipeline.md) - a sample that demonstrates how to create a **GStreamer** pipeline for extracting and decoding **MISB601 KLV** metadata from **STANAG 4609** files/streams using the **MisbCoreNative** library.  

[GStreamer encode](./DemoApps/gst-klv-encode-pipeline.md) - a sample that demonstrates how to create a simplified **GStreamer** pipeline for encoding and injecting **MISB601** KLV metadata into **STANAG 4609** files/streams using **MisbCoreNative** library.  