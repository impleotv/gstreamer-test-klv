# MisbCoreNative Gstreamer test applications

 GStreamer provides basic support for muxing and demuxing KLV metadata into/from STANAG 4609 files and streams.  
 This repository contains sample GStreamer pipelines for insertion and extraction of the metadata using MisbCoreNative library. 

 ## KlvEncode

  Demonstrates how to create a test H.264 video and multiplex it into standard compliant STANAG 4609 stream by encodeing MISB 601 metadata and injecting it into the stream

 ## KlvDecode

 Demonstrates how to extract and decode MISB601 KLV encoded metadata from STANAG 4609 file

