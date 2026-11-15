#!/usr/bin/env python3
"""Packs background music for PocketPKMN: an intro played once and a loop played forever (tools/find_loop.py).

usage: python3 -I tools/make_music.py <in.wav> <loop start> <loop end> <out music.bin> [preview.wav]
  in.wav      16-bit stereo, 32728 Hz (the 3DS DSP's rate), e.g. ffmpeg -ar 32728 -ac 2 -c:a pcm_s16le
  music.bin   "PPMU" u32 rate, u16 channels, u16 0, u32 intro samples, u32 loop samples, then interleaved
              little-endian PCM16 for intro + loop. Read by source/sound.c from sdmc:/3ds/pocketpkmn/music.bin.
  preview     intro + loop 3 times (to listen to the seam on the PC)
The music file is the user's own and goes on their SD card only; it is never built into the app or the release.
"""
import struct
import sys

import numpy as np
from scipy.io import wavfile


def main():
    rate, data = wavfile.read(sys.argv[1])
    start, end = int(sys.argv[2]), int(sys.argv[3])
    if data.ndim != 2 or data.dtype != np.int16:
        raise SystemExit("need 16-bit stereo")
    body = data[:end]
    with open(sys.argv[4], "wb") as f:
        f.write(b"PPMU" + struct.pack("<IHHII", rate, 2, 0, start, end - start))
        f.write(body.astype("<i2").tobytes())
    print("wrote %s: intro %.3f s, loop %.3f s, %d bytes" % (sys.argv[4], start / rate, (end - start) / rate,
                                                          16 + body.nbytes))
    if len(sys.argv) > 5:
        loop = data[start:end]
        prev = np.concatenate([data[:start], loop, loop, loop])
        wavfile.write(sys.argv[5], rate, prev)
        print("preview %s: %.1f s (seams at %.2f s and %.2f s)" % (sys.argv[5], len(prev) / rate, end / rate,
                                                                  (end + end - start) / rate))


if __name__ == "__main__":
    main()
