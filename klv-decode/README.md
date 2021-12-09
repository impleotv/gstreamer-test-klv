# Decode Klv packets GStreamer sample

```bash
gst-launch-1.0 filesrc location=2t.ts ! tsdemux name=demux demux. ! queue ! h264parse ! 'video/x-h264, stream-format=byte-stream, alignment=au' ! avdec_h264 ! autovideosink demux. ! queue ! 'meta/x-klv' ! filesink location=data.bin

```