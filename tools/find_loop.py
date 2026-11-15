#!/usr/bin/env python3
"""Finds a seamless loop in a piece of game music that plays its loop more than once (then fades).

usage: python3 -I tools/find_loop.py <in.wav>
Prints the loop length (the repeat lag), the loop start / end in samples, and how well the two copies match.
Method: autocorrelation of the mono signal (decimated) gives the repeat lag L; the loop start S is the earliest
point from which x[t] == x[t + L] holds for a long stretch; both are then refined to the sample on the full-rate
signal. Playing intro [0, S) once and then [S, S + L) forever reproduces the piece without the fade.
"""
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import fftconvolve


def main():
    rate, data = wavfile.read(sys.argv[1])
    x = data.astype(np.float64)
    mono = x.mean(axis=1) if x.ndim == 2 else x
    mono /= np.abs(mono).max() + 1e-9
    dec = 8
    m = mono[: len(mono) // dec * dec].reshape(-1, dec).mean(axis=1)
    r = rate / dec
    n = len(m)
    # autocorrelation, normalised by the overlap length
    ac = fftconvolve(m, m[::-1], mode="full")[n - 1:]
    overlap = np.arange(n, 0, -1)
    acn = ac / overlap
    lo, hi = int(15 * r), int(0.75 * n)
    lag = lo + int(np.argmax(acn[lo:hi]))
    print("repeat lag ~ %.3f s" % (lag / r))
    # earliest start S where 4 s windows of x[t] and x[t+lag] match
    win = int(4 * r)
    best = None
    for s in range(0, n - lag - win, int(0.05 * r)):
        a, b = m[s:s + win], m[s + lag:s + lag + win]
        c = np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-9)
        if c > 0.98:
            best = s
            break
    if best is None:
        print("no start found with correlation > 0.98")
        return 1
    # refine on the full-rate signal: lag to the sample around lag*dec, start to the sample around best*dec
    L0, S0 = lag * dec, best * dec
    seg = mono[S0:S0 + 2 * rate]
    bestL, bestc = L0, -1
    for L in range(L0 - 2 * dec, L0 + 2 * dec + 1):
        b = mono[S0 + L:S0 + L + 2 * rate]
        c = np.dot(seg, b) / (np.linalg.norm(seg) * np.linalg.norm(b) + 1e-9)
        if c > bestc:
            bestL, bestc = L, c
    # start: a quiet spot where both copies agree best within the matching region (minimise the jump)
    S = S0
    lo_s, hi_s = S0, min(S0 + 4 * rate, len(mono) - bestL - 1)
    diff = np.abs(mono[lo_s:hi_s] - mono[lo_s + bestL:hi_s + bestL]) + 0.1 * np.abs(mono[lo_s:hi_s])
    S = lo_s + int(np.argmin(diff))
    E = S + bestL
    # how well the whole loop body matches its repeat (if the file holds a full second copy)
    tail = min(len(mono) - E, bestL)
    a, b = mono[S:S + tail], mono[E:E + tail]
    whole = np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-9)
    print("rate %d, samples %d (%.2f s)" % (rate, len(mono), len(mono) / rate))
    print("loop start %d (%.3f s), loop end %d (%.3f s), length %d (%.3f s)" % (S, S / rate, E, E / rate, bestL, bestL / rate))
    print("match: 2 s at start %.4f, over %.1f s of the repeat %.4f" % (bestc, tail / rate, whole))
    print("jump at the loop point: %.5f (full scale 1)" % abs(mono[E] - mono[S]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
