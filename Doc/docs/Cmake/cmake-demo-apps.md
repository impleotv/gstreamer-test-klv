# Demo applications


> Note. You have to download and set up a path to  **MisbCoreNativeLib.so**  (**MisbCoreNativeLib.dll**, for Windows):

```C
const char *PathToLibrary = "../bin/linux-x64/MisbCoreNativeLib.so";
```

 or copy it to the application's target directory.   

The latest version can be found [here](https://github.com/impleotv/misbcore-native-lib-release).



## GStreamer Klv Decode

This sample application demonstrates how to create a **GStreamer** pipeline for extracting and decoding ***MISB601** KLV metadata from **STANAG 4609** files/streams using the **MisbCoreNative** library.  
[Code Walkthrough](../DemoApps/gst-klv-decode-pipeline.md)


## GStreamer Klv Encode

This sample application demonstrates how to create a simplified **GStreamer** pipeline for encoding and injecting **MISB601** KLV metadata into **STANAG 4609** files/streams using **MisbCoreNative** library.  
[Code Walkthrough](../DemoApps/gst-klv-encode-pipeline.md)

