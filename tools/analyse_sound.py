#!/usr/bin/env python3
"""Describes a short sound clip for re-creating its STYLE (not its audio): loudness over time (onsets / rhythm),
the strongest pitches per slice, and a spectrogram picture.

usage: python3 -I tools/analyse_sound.py <in.wav> <out.png>
"""
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import stft


def name(f):
    if f <= 0:
        return "-"
    n = 69 + 12 * np.log2(f / 440.0)
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    r = int(round(n))
    return "%s%d%+d" % (names[r % 12], r // 12 - 1, int(round((n - r) * 100)))


def main():
    rate, x = wavfile.read(sys.argv[1])
    x = x.astype(np.float64)
    if x.ndim == 2:
        x = x.mean(axis=1)
    x /= np.abs(x).max() + 1e-9
    print("rate %d, %.3f s" % (rate, len(x) / rate))
    hop = int(0.01 * rate)
    f, t, Z = stft(x, rate, nperseg=4096, noverlap=4096 - hop)
    mag = np.abs(Z)
    env = mag.sum(axis=0)
    env /= env.max()
    # onsets: rises in the envelope
    d = np.diff(env, prepend=env[0])
    on = [i for i in range(1, len(d) - 1) if d[i] > 0.08 and d[i] >= d[i - 1] and d[i] >= d[i + 1]]
    print("onsets (s):", " ".join("%.2f" % t[i] for i in on))
    if len(on) > 2:
        iv = np.diff([t[i] for i in on])
        print("onset spacing: median %.3f s, min %.3f, max %.3f" % (np.median(iv), iv.min(), iv.max()))
    # strongest peaks per 50 ms slice (above 150 Hz)
    lo = np.searchsorted(f, 150)
    for k in range(0, len(t), 5):
        col = mag[lo:, k]
        idx = np.argsort(col)[-4:][::-1]
        peaks = ["%s(%.0f)" % (name(f[lo + i]), f[lo + i]) for i in idx if col[i] > 0.05 * mag[lo:].max()]
        print("%5.2f  env %.2f  %s" % (t[k], env[k], "  ".join(peaks)))
    # average spectrum: where the energy is
    avg = mag.mean(axis=1)
    bands = [(150, 500), (500, 1000), (1000, 2000), (2000, 4000), (4000, 8000), (8000, 16000)]
    tot = avg.sum()
    print("energy by band:", ", ".join("%d-%d Hz %.0f%%" % (a, b, 100 * avg[(f >= a) & (f < b)].sum() / tot)
                                       for a, b in bands))
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig = plt.figure(figsize=(12, 6))
        plt.pcolormesh(t, f, 20 * np.log10(mag + 1e-6), shading="auto", vmin=-80, vmax=0)
        plt.ylim(0, 8000)
        plt.xlabel("s")
        plt.ylabel("Hz")
        plt.colorbar()
        fig.savefig(sys.argv[2], dpi=90)
        print("picture", sys.argv[2])
    except ImportError:
        print("(no matplotlib: no picture)")


if __name__ == "__main__":
    main()
