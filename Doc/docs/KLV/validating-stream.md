
# Validating STANAG stream


We'll be using  <a href="https://www.impleotv.com/content/stplayer/help/index.html">StPlayer application</a> to validate the created stream.

Let's load the recorded file and see what is inside the multiplex.


We can see that the resulted output is a TS file with 2 elementary streams.
![TS Stream](images/TsInfo.png)

Video / metadata packet distribution in the file.
![Stream Pie](images/StreamPie.png)

There is a video elementary stream
![Video stream](images/VideoInfo.png)

and Klv data stream
![Klv data Stream](images/KlvInfo.png)

Intervals between the Video timestamps (Pts / Dts) show a perfect line.
![Pts intervals](images/StreamPtsIntervals.png)

And as we've inserted a Klv packet with every video frame, we see the exact correlation in the stream - our video and klvs are in a perfect frame-accurate sync.
![Video / Klv sync](images/StreamVideoDataPts.png)