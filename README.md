# medialint

`medialint` is a fast, minimal(-ish) media linter tool, intended to find issues in your media library.

It checks for the following issues:

 - preferred container / codecs / quality
 - incorrect file naming / structure
 - missing subtitles

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
```

## Constraints

- Platform limitations:

  `medialint` currently only supports running on Unix-like platforms. The following set of things must be addressed before it works on other platforms.

  - [ ] Replace `nftw` with something more portable.
  - [ ] Figure out how to cross-compile GLib for other platforms.

## License

Copyright (c) 2024 Tom Richards. All rights reserved.
