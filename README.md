# medialint

`medialint` is a fast, minimal(-ish) media linter tool, intended to find issues in your media library.

It checks for the following issues:

 - preferred container / codecs / quality
 - incorrect file naming / structure
 - missing subtitles / preferred default subtitle language

## Features

 - [x] Supports a wide range of media types via `libavformat`.
 - [x] Processes files in parallel for maximum speed.

## System library requirements

 - `libavcodec` (FFmpeg)
 - `libavformat` (FFmpeg)
 - `libavutil` (FFmpeg)
 - GLib 2.0

## Getting started

```bash
# Build
make

# Run
bin/medialint <dir>
...
/youtube/demo.mkv: Subtitles/Presence: No subtitles found.
/youtube/demo.mkv: Video/Bitrate: 0.29 Mibps [track 0].
/youtube/demo.mkv: Video/Resolution: 450x360 [track 0].
Time:      0.01 seconds
Total:     3
Processed: 3
Errors:    1

```

## Downloads

For now, compiled binaries are available as build artifacts from the `build` workflow:

https://github.com/t-richards/medialint/actions/workflows/build.yml

Select the most recent run at the top of the list, then scroll down to the "artifacts" section. In the future, we may choose to promote these build artifacts as a proper release.

## License

Copyright (c) 2024 Tom Richards. All rights reserved.
