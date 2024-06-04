


On Ubuntu, follow the steps:

Compile at least 1.24.4 to have mpegtsmux fixed

ubuntu:latest

# Install necessary tools and dependencies
sudo apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    pkg-config \
    ninja-build \
    libglib2.0-dev \
    liborc-0.4-dev \
    libpango1.0-dev \
    libgtk-3-dev \
    git \
    sudo \
    python3 \
    python3-pip \
    meson \
    flex \
    bison \
    nasm \



## Clone gstreamer

git clone https://gitlab.freedesktop.org/gstreamer/gstreamer.git
cd gstreamer
git checkout 1.24.4


## 
// Enable x264

meson -Dgpl=enabled -Dugly=enabled -Dgst-plugins-ugly:x264=enabled builddir 
meson setup  --prefix=/opt/gstreamer/1.24.4 builddir
meson compile -C builddir
meson install -C builddir     

## 
Setup the paths:

export GSTREAMER_ROOT=/opt/gstreamer/1.24.4

# Add GStreamer binary directory to PATH
export PATH=$GSTREAMER_ROOT/bin:$PATH

# Add GStreamer libraries to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$GSTREAMER_ROOT/lib:$GSTREAMER_ROOT/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

# Add GStreamer plugins to GST_PLUGIN_PATH
export GST_PLUGIN_PATH=$GSTREAMER_ROOT/lib/gstreamer-1.0:$GST_PLUGIN_PATH
