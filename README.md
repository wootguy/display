# display

## Install notes
1. pip3 install yt_dlp
1. Install the MediaPlayer metamod plugin
1. Add MetaHelper.as as an angelscript plugin

The metamod plugin expects `ffmpeg` and `python3` (or `python` for windows) to be usable shell commands.

building the metamod plugin (linux):
```
apt install libopus-dev:i386
git clone --recurse-submodules https://github.com/wootguy/display
cd display && mkdir build && cd build
cmake ..
cmake --build . --config Release
```
