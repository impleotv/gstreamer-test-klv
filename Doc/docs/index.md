
# MisbCoreNative with GStreamer

## ⚠️ MisbCoreNative Version Compatibility Notice  

Please note that as of version 3.0.0, the **MisbCoreNative** library has been renamed to **libmisbcore**.  
You may either use an earlier version or update your references accordingly.


## Introduction

**MisbCoreNative** can be used with C/C++ GStreamer applications.
We provide simplified code samples that demonstrate how to use the library to encode or decode MISB KLV data with GStreamer.

<div align="center">
	<font color="blue">STANAG 4609 / MISB demo applications.</font>
</div>

## Demo apps

[misbcore-test](VisualStudio/vs-demo-apps.md) - basic C++ console app that demonstrates how to load **MisbCoreNative** library and perform encoding/decoding MISB601 metadata.


[GStreamer decode](./DemoApps/gst-klv-decode-pipeline.md) - a sample that demonstrates how to create a **GStreamer** pipeline for extracting and decoding **MISB601 KLV** metadata from **STANAG 4609** files/streams using the **MisbCoreNative** library.  

[GStreamer encode](./DemoApps/gst-klv-encode-pipeline.md) - a sample that demonstrates how to create a simplified **GStreamer** pipeline for encoding and injecting **MISB601** KLV metadata into **STANAG 4609** files/streams using **MisbCoreNative** library.  