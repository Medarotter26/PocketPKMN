#!/usr/bin/env python3
"""PocketPKMN's own sound effects, synthesised here (no game audio): button click, transfer running (loops),
transfer done, transfer failed.

usage: python3 -I tools/make_sfx.py [out dir] [preview dir]
  out dir      default romfs/sound: click.bin, transfer.bin, success.bin, fail.bin - same "PPMU" format as
               tools/make_music.py (mono; transfer.bin is all loop, the others all intro = play once)
  preview dir  optional: the same sounds as .wav to listen to on the PC
Small band-limited square / triangle voices with soft envelopes, 32728 Hz (the 3DS DSP's rate).
"""
import os
import struct
import sys

import numpy as np
from scipy.io import wavfile

RATE = 32728
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def note(n):  # MIDI note -> Hz
    return 440.0 * 2 ** ((n - 69) / 12)


def square(f, t, harmonics=9):
    """band-limited square (odd harmonics only, so no aliasing hiss)"""
    out = np.zeros_like(t)
    for k in range(1, 2 * harmonics, 2):
        if f * k > RATE / 2 * 0.9:
            break
        out += np.sin(2 * np.pi * f * k * t) / k
    return out * (4 / np.pi) * 0.6


def triangle(f, t, harmonics=7):
    out = np.zeros_like(t)
    for i, k in enumerate(range(1, 2 * harmonics, 2)):
        if f * k > RATE / 2 * 0.9:
            break
        out += ((-1) ** i) * np.sin(2 * np.pi * f * k * t) / (k * k)
    return out * (8 / np.pi ** 2)


def env(n, attack=0.004, release=0.05, decay=None):
    t = np.arange(n) / RATE
    e = np.ones(n)
    a = int(attack * RATE)
    if a:
        e[:a] = np.linspace(0, 1, a)
    if decay:
        e *= np.exp(-t / decay)
    r = int(release * RATE)
    if r:
        e[-r:] *= np.linspace(1, 0, r)
    return e


def tone(freq, dur, voice=square, vol=0.5, **kw):
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    return voice(freq, t) * env(n, **kw) * vol


# v2 (user: "Those sound effects are annoying"): no square waves, no busy patterns. Soft sine / bell voices with
# rounded attacks, quieter, and the transfer sound is one gentle chime per second instead of constant pips.

def bell(freq, dur, vol=0.3, decay=0.35, attack=0.006):
    """a soft bell: sine plus a quiet octave and a faint inharmonic partial, smooth attack, exponential decay"""
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    s = np.sin(2 * np.pi * freq * t) + 0.18 * np.sin(2 * np.pi * 2 * freq * t) * np.exp(-t / (decay * 0.5)) \
        + 0.05 * np.sin(2 * np.pi * 2.76 * freq * t) * np.exp(-t / (decay * 0.3))
    a = max(1, int(attack * RATE))
    e = np.exp(-t / decay)
    e[:a] *= 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, a))  # smooth (raised cosine) attack: no click
    r = int(0.03 * RATE)
    e[-r:] *= np.linspace(1, 0, r)
    return s * e * vol


def place(total, events):
    out = np.zeros(int(total * RATE))
    for at, snd in events:
        i = int(at * RATE)
        n = min(len(snd), len(out) - i)
        out[i:i + n] += snd[:n]
    return out


def click():
    # a small, quiet "pop": a short sine with a quick downward pitch bend
    n = int(0.03 * RATE)
    t = np.arange(n) / RATE
    f = 880 * np.exp(-t / 0.02)
    f = np.maximum(f, 520)
    phase = 2 * np.pi * np.cumsum(f) / RATE
    e = np.exp(-t / 0.008)
    a = int(0.0015 * RATE)
    e[:a] *= np.linspace(0, 1, a)
    return np.sin(phase) * e * 0.22


def transfer():
    # one soft two-note chime per 1.2 s (E6 then B6, quiet), then silence; loops seamlessly (ends silent)
    return place(1.2, [(0.0, bell(note(88), 0.5, vol=0.11, decay=0.18)),
                       (0.09, bell(note(95), 0.5, vol=0.08, decay=0.16))])


def success():
    # a warm rising chime: G5, C6, E6 overlapping, ringing out together
    return place(1.1, [(0.0, bell(note(79), 1.0, vol=0.20, decay=0.45)),
                       (0.10, bell(note(84), 0.95, vol=0.20, decay=0.45)),
                       (0.20, bell(note(88), 0.9, vol=0.22, decay=0.5))])


def fail():
    # a gentle falling pair, low and soft: E5 then C5 (no buzz)
    return place(0.75, [(0.0, bell(note(76), 0.4, vol=0.20, decay=0.18)),
                        (0.16, bell(note(72), 0.58, vol=0.22, decay=0.3))])


# --- v3 (user: "I want a like ambient new age technological sound") -------------------------------------------------
# Stereo. Glassy FM bells, soft detuned pads, airy filtered noise, long smooth reverb tails. These are the sounds used.

RNG = np.random.default_rng(7)  # fixed seed: the same files every run


def lowpass(x, cutoff):
    """one-pole low-pass along the last axis"""
    a = np.exp(-2 * np.pi * cutoff / RATE)
    from scipy.signal import lfilter
    return lfilter([1 - a], [1, -a], x, axis=-1)


def fm_bell(f, dur, ratio=3.5, index=2.2, idecay=0.12, adecay=0.5, vol=0.3):
    """glassy FM bell: the modulation fades faster than the tone, so it starts bright and rings out pure"""
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    mod = index * np.exp(-t / idecay) * np.sin(2 * np.pi * f * ratio * t)
    s = np.sin(2 * np.pi * f * t + mod)
    e = np.exp(-t / adecay)
    a = max(2, int(0.004 * RATE))
    e[:a] *= 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, a))
    r = min(n // 4, int(0.05 * RATE))
    e[-r:] *= np.linspace(1, 0, r)
    return s * e * vol


def pan(mono, p):
    """p -1 (left) .. 1 (right), equal power -> (2, n)"""
    ang = (p + 1) * np.pi / 4
    return np.stack([mono * np.cos(ang), mono * np.sin(ang)])


def reverb(x, seconds=1.6, wet=0.35, circular=False):
    """stereo reverb: convolution with decaying, darkened noise (a different noise per side for width)"""
    from scipy.signal import fftconvolve
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    ir = RNG.standard_normal((2, n)) * np.exp(-t / (seconds / 5))
    ir = lowpass(ir, 5000)
    ir[:, :int(0.012 * RATE)] = 0  # a short gap before the reflections
    ir /= np.sqrt((ir ** 2).sum(axis=1, keepdims=True)) + 1e-9
    if circular:  # for a loop: the tail wraps round to the start, so the loop has no seam
        L = x.shape[1]
        out = np.zeros_like(x)
        for c in range(2):
            y = fftconvolve(x[c], ir[c])
            for k in range(0, len(y), L):
                seg = y[k:k + L]
                out[c, :len(seg)] += seg
        return (1 - wet) * x + wet * out
    y = np.stack([fftconvolve(x[c], ir[c]) for c in range(2)])
    dry = np.zeros_like(y)
    dry[:, :x.shape[1]] = x
    return (1 - wet) * dry + wet * y


def place2(total, events):
    out = np.zeros((2, int(total * RATE)))
    for at, snd in events:
        i = int(at * RATE)
        n = min(snd.shape[1], out.shape[1] - i)
        out[:, i:i + n] += snd[:, :n]
    return out


def air_sweep(dur, f0, f1, vol=0.05):
    """airy 'whoosh': noise through a band that slides from f0 to f1, soft in and out"""
    n = int(dur * RATE)
    noise = RNG.standard_normal((2, n))
    out = np.zeros_like(noise)
    from scipy.signal import butter, sosfilt
    steps = 24
    for k in range(steps):  # piecewise band-pass sweep
        a, b = k * n // steps, (k + 1) * n // steps
        fc = f0 * (f1 / f0) ** (k / (steps - 1))
        sos = butter(2, [fc * 0.7 / (RATE / 2), min(0.95, fc * 1.4 / (RATE / 2))], btype="band", output="sos")
        out[:, a:b] = sosfilt(sos, noise[:, a:b], axis=1)
    e = np.sin(np.linspace(0, np.pi, n)) ** 2
    return out * e * vol


def norm(x, peak):
    return x * (peak / (np.abs(x).max() + 1e-9))


def click3():
    # a tiny glass "tik": high FM bell, very short, with a little room around it
    b = fm_bell(note(96), 0.08, ratio=2.0, index=1.2, idecay=0.01, adecay=0.018, vol=1.0)  # C7
    x = place2(0.32, [(0.0, pan(b, 0.0))])
    return norm(reverb(x, 0.4, wet=0.25)[:, :int(0.32 * RATE)], 0.22)


def transfer3():
    # v3b (user: "the connecting sound is a bit annoying"): calmer - a slow, warm, breathing pad (Cmaj9 colour) and
    # only one faint glass note every 3.2 s (was 8 twinkles per 4.8 s); quieter overall. 9.6 s, seamless.
    L = 9.6
    n = int(L * RATE)
    t = np.arange(n) / RATE
    pad = np.zeros((2, n))
    # v3c (user: "I can't really hear the transfer sound effect"): back up an octave (C4..E5 - small speakers lose
    # the low notes), brighter filter, louder twinkles and a higher level
    for k, nt in enumerate((60, 67, 71, 74, 76)):  # C4 G4 B4 D5 E5
        for side, det in ((0, -0.12), (1, 0.12)):  # slight detune per side = width
            f = round((note(nt) + det) * L) / L  # whole cycles in the loop: no seam
            swell = 0.55 + 0.45 * np.sin(2 * np.pi * t / L * (1 + k % 2) + k)  # slow breathing
            pad[side] += np.sin(2 * np.pi * f * t + k) * swell / 5
    pad = lowpass(pad, 1800)
    twinkle = []
    for i, nt in enumerate((84, 91, 88)):  # C6, G6, E6: one every 3.2 s
        b = fm_bell(note(nt), 1.2, ratio=3.5, index=0.9, idecay=0.06, adecay=0.4, vol=0.30)
        twinkle.append((i * L / 3 + 0.4, pan(b, (-0.5, 0.5, 0.0)[i])))
    x = pad + place2(L, twinkle)
    x = reverb(x, 2.8, wet=0.45, circular=True)
    return norm(x, 0.45)


def transfer4():
    # v4 (user: "Rethink the transfer sound effect. I'd like something electrical sounding. Light and electrical"):
    # a faint, high shimmering hum (a charged glow), soft static crackles, and tiny quick 'zip' chirps like data
    # packets, panned left / right. No musical notes. 4.8 s, seamless (every part wraps round the loop).
    L = 4.8
    n = int(L * RATE)
    t = np.arange(n) / RATE
    from scipy.signal import butter, sosfilt
    x = np.zeros((2, n))
    # 1. the glow: detuned high sines with a fast flutter (whole cycles in the loop)
    for side, f0 in ((0, 2400.0), (1, 2412.5)):
        f = round(f0 * L) / L
        flutter = 0.5 + 0.5 * np.sin(2 * np.pi * (round(11 * L) / L) * t + side)
        x[side] += np.sin(2 * np.pi * f * t) * (0.25 + 0.75 * flutter) * 0.035
    # 2. static: short, high, soft noise ticks at random places (about 22 per second)
    hp = butter(2, 3500 / (RATE / 2), btype="high", output="sos")
    for _ in range(int(22 * L)):
        at = RNG.integers(0, n)
        ln = int(RNG.uniform(0.0008, 0.003) * RATE)
        tick = sosfilt(hp, RNG.standard_normal(ln)) * np.exp(-np.arange(ln) / (ln / 3)) * RNG.uniform(0.04, 0.12)
        p = RNG.uniform(-0.8, 0.8)
        st = pan(tick, p)
        idx = (at + np.arange(ln)) % n  # wraps round: seamless
        x[:, idx] += st
    # 3. zips: quick rising chirps (1.6 -> 4.5 kHz in 35 ms), one every 0.6 s, alternating sides
    zn = int(0.035 * RATE)
    zt = np.arange(zn) / RATE
    fz = 1600 * (4500 / 1600) ** (zt / zt[-1])
    zip_ = np.sin(2 * np.pi * np.cumsum(fz) / RATE) * np.sin(np.linspace(0, np.pi, zn)) ** 2 * 0.10
    for i in range(8):
        at = int((i * 0.6 + 0.1) * RATE)
        idx = (at + np.arange(zn)) % n
        x[:, idx] += pan(zip_ * (0.8 if i % 2 else 1.0), -0.6 if i % 2 == 0 else 0.6)
    x = reverb(x, 0.9, wet=0.3, circular=True)
    return norm(x, 0.40)


def transfer5():
    # v5 (user on v4: "Too high pitch and it sounds like water drips"): lower and continuous - a soft electric hum
    # (band-limited saw on A3 / E4, filtered, detuned L/R), a flowing 'current' of mid-band noise that flickers, and
    # three short spark crackles per loop (dense, mid-band ticks - no chirps, nothing very high). 4.8 s, seamless.
    L = 4.8
    n = int(L * RATE)
    t = np.arange(n) / RATE
    from scipy.signal import butter, sosfilt, sosfiltfilt
    x = np.zeros((2, n))
    # 1. hum: saw waves (first 12 harmonics) on A3 and E4, whole cycles in the loop, a gentle 6 Hz tremble
    for side, det in ((0, -0.4), (1, 0.4)):
        for base, lvl in ((note(57), 1.0), (note(64), 0.45)):
            f = round((base + det) * L) / L
            saw = sum(np.sin(2 * np.pi * f * k * t) / k for k in range(1, 13))
            trem = 1 + 0.15 * np.sin(2 * np.pi * (round(6 * L) / L) * t + side)
            x[side] += saw * trem * lvl * 0.05
    x = lowpass(lowpass(x, 1400), 1400)
    # 2. current: noise in the 500-1600 Hz band, slowly swelling (one swell per loop) with a quick flicker
    bp = butter(2, [500 / (RATE / 2), 1600 / (RATE / 2)], btype="band", output="sos")
    cur = np.stack([sosfiltfilt(bp, RNG.standard_normal(n)) for _ in range(2)])
    cur = cur - cur.mean(axis=1, keepdims=True)
    swell = 0.55 + 0.45 * np.sin(2 * np.pi * t / L)
    flick = 0.8 + 0.2 * np.sin(2 * np.pi * (round(17 * L) / L) * t)
    xf = int(0.05 * RATE)  # cross-fade the noise ends so the loop has no seam
    ramp = np.linspace(0, 1, xf)
    cur[:, :xf] = cur[:, :xf] * ramp + cur[:, -xf:] * (1 - ramp)
    cur = cur[:, :n]
    x += cur * swell * flick * 0.06
    # 3. sparks: three short bursts of dense crackle (800 Hz - 4.5 kHz), 90 ms each
    band = butter(2, [800 / (RATE / 2), 4500 / (RATE / 2)], btype="band", output="sos")
    for k, (at_s, p) in enumerate(((0.7, -0.4), (2.3, 0.4), (3.9, 0.0))):
        bn = int(0.09 * RATE)
        burst = np.zeros(bn)
        for _ in range(40):
            i = RNG.integers(0, bn - 40)
            ln = RNG.integers(8, 40)
            burst[i:i + ln] += RNG.standard_normal(ln) * RNG.uniform(0.3, 1.0)
        burst = sosfilt(band, burst) * np.sin(np.linspace(0, np.pi, bn)) * 0.09
        idx = (int(at_s * RATE) + np.arange(bn)) % n
        x[:, idx] += pan(burst, p)
    x = reverb(x, 0.8, wet=0.25, circular=True)
    return norm(x, 0.40)


def transfer6():
    # v6 (user pointed at HGSS's own walker transfer sound; the clip they gave was measured with
    # tools/analyse_sound.py, its audio is NOT used): the same INGREDIENTS in our own notes and key - phrases of a
    # hollow electronic tone (fundamental + strong octave) that holds, flicks up briefly and drops back, under a bright
    # metallic shimmer (clustered partials ~4.4-5.6 kHz) trilling ~16 times a second that swells with each phrase;
    # short gaps between phrases. Ours: D5 phrase, A5 blip, A5 phrase (the clip: E5 / B5). 3.0 s, seamless.
    L = 3.0
    n = int(L * RATE)
    x = np.zeros((2, n))

    def phrase(start, dur, steps, peak_at=0.6, shimmer_vol=0.05, tone_vol=0.20):
        """steps: [(time from start, MIDI note)], a quick 10 ms glide between them"""
        m = int(dur * RATE)
        t = np.arange(m) / RATE
        f = np.full(m, note(steps[0][1]))
        for at, nt in steps[1:]:
            i = int(at * RATE)
            g = min(m - i, int(0.01 * RATE))
            if i >= m:
                continue
            f[i:i + g] = np.linspace(f[i - 1] if i else note(nt), note(nt), g)
            f[i + g:] = note(nt)
        ph = 2 * np.pi * np.cumsum(f) / RATE
        tone = np.sin(ph) + 0.8 * np.sin(2 * ph) + 0.15 * np.sin(3 * ph)  # hollow: strong octave
        # swell: rises to its peak at peak_at of the phrase, then fades (smooth ends)
        pk = peak_at * dur
        env = np.where(t < pk, (t / pk) ** 1.5, np.exp(-(t - pk) / (0.35 * (dur - pk) + 1e-9)))
        env *= np.sin(np.linspace(0, np.pi, m)) ** 0.3
        # shimmer: inharmonic high cluster, pulsed ~16.5 times a second (each pulse a quick hit + 22 ms decay)
        cl = sum(np.sin(2 * np.pi * fr * t + RNG.uniform(0, 6.28)) for fr in (4410, 4705, 5010, 5290, 5560)) / 5
        pulse_t = (t * 16.5) % 1.0 / 16.5
        pulses = np.exp(-pulse_t / 0.022)
        sh = cl * pulses * env * shimmer_vol / 0.5
        out = np.zeros((2, n))
        i0 = int(start * RATE)
        idx = (i0 + np.arange(m)) % n
        out[:, idx] += pan(tone * env * tone_vol, -0.15)
        out[:, idx] += pan(sh, 0.25)
        return out

    x += phrase(0.15, 1.15, [(0.0, 74), (0.34, 77), (0.40, 74), (0.62, 76), (0.82, 74)])  # D5 .. F5 flick .. E5 .. D5
    x += phrase(1.32, 0.28, [(0.0, 83), (0.08, 81)], peak_at=0.3, shimmer_vol=0.03, tone_vol=0.12)  # B5 -> A5 blip
    x += phrase(1.70, 1.20, [(0.0, 81), (0.30, 77), (0.36, 81)], peak_at=0.45)  # A5, F5 flick, A5 held
    x = reverb(x, 0.7, wet=0.22, circular=True)
    return norm(x, 0.45)


def transfer7():
    # v7 (user: "just give me like a very softened beep pulsing like a beep beep beep"): a pure A5 sine beep, 100 ms
    # with a rounded (raised-cosine) rise and fall, one every 0.4 s, a touch of room. 2.4 s loop (6 beeps), seamless.
    L, period, beep = 2.4, 0.4, 0.10
    n = int(L * RATE)
    m = int(beep * RATE)
    t = np.arange(m) / RATE
    env = np.sin(np.linspace(0, np.pi, m)) ** 2  # soft in and out
    b = np.sin(2 * np.pi * note(81) * t) * env
    x = np.zeros((2, n))
    # v7b (user: "Can you have more gap between the series of [pulses]?"): groups of 3 beeps, then a ~1.5 s gap
    for i in range(3):
        idx = (int(i * period * RATE) + np.arange(m)) % n
        x[:, idx] += pan(b, 0.0)
    x = reverb(x, 0.5, wet=0.18, circular=True)
    return norm(x, 0.85)  # v7c (console: "really quiet"): 0.85 peak (was 0.35)


def success3():
    # glassy rising arpeggio (Cmaj9) over an airy upward whoosh, ringing out in a big soft room
    events = [(0.0, air_sweep(0.9, 400, 6000, vol=0.08))]
    for i, nt in enumerate((72, 76, 79, 83, 86)):  # C5 E5 G5 B5 D6
        b = fm_bell(note(nt), 1.4, ratio=3.5, index=2.0, idecay=0.08, adecay=0.55, vol=0.32)
        events.append((0.07 * i, pan(b, -0.5 + 0.25 * i)))
    x = place2(1.6, events)
    y = reverb(x, 2.4, wet=0.4)
    return norm(y[:, :int(2.4 * RATE)], 0.42)


def fail3():
    # two soft glass tones sinking (A4 -> E4) with a gentle downward 'power down' shimmer
    a = fm_bell(note(69), 0.9, ratio=1.41, index=1.6, idecay=0.1, adecay=0.35, vol=0.34)
    n = int(1.0 * RATE)
    t = np.arange(n) / RATE
    f = note(64) * (1 - 0.05 * t)  # slides a little lower as it fades
    ph = 2 * np.pi * np.cumsum(f) / RATE
    b = np.sin(ph + 1.2 * np.exp(-t / 0.1) * np.sin(1.41 * ph)) * np.exp(-t / 0.45) * 0.36
    b[:int(0.004 * RATE)] *= np.linspace(0, 1, int(0.004 * RATE))
    b[-int(0.05 * RATE):] *= np.linspace(1, 0, int(0.05 * RATE))
    x = place2(1.3, [(0.0, pan(a, -0.25)), (0.22, pan(b, 0.25)), (0.05, air_sweep(0.6, 3000, 500, vol=0.04))])
    y = lowpass(reverb(x, 2.0, wet=0.4), 4500)
    return norm(y[:, :int(2.0 * RATE)], 0.36)


def pcm(x):
    x = np.clip(x, -1, 1)
    return (x * 32767 * 0.9).astype("<i2")


def write_bin(path, samples, loop):
    """samples: (n,) mono or (2, n) stereo"""
    st = samples.ndim == 2
    s = pcm(samples.T if st else samples)  # stereo -> interleaved
    frames = s.shape[0]
    intro, lp = (0, frames) if loop else (frames, 0)
    with open(path, "wb") as f:
        f.write(b"PPMU" + struct.pack("<IHHII", RATE, 2 if st else 1, 0, intro, lp))
        f.write(s.tobytes())
    return frames


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "romfs", "sound")
    prev = sys.argv[2] if len(sys.argv) > 2 else None
    os.makedirs(out, exist_ok=True)
    sounds = {"click": (click3(), False), "transfer": (transfer7(), True), "success": (success3(), False),
              "fail": (fail3(), False)}
    for name, (s, loop) in sounds.items():
        n = write_bin(os.path.join(out, name + ".bin"), s, loop)
        print("%s.bin: %.3f s%s, %s" % (name, n / RATE, " (loops)" if loop else "", "stereo" if s.ndim == 2 else "mono"))
        if prev:
            os.makedirs(prev, exist_ok=True)
            data = np.tile(s, (1, 3)) if loop else s
            wavfile.write(os.path.join(prev, name + ".wav"), RATE, pcm(data.T))


if __name__ == "__main__":
    main()
