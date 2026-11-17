#!/usr/bin/env python3
"""PocketPKMN's own background music, "Pocket Stroll" - composed for the app and rendered from this file
(no samples, no recordings: every sound is synthesised here). Relaxed, warm, F major.

In the app (v0.73): python3 -I tools/compose_music.py romfs/sound/music.bin  (the defaults below = the chosen take:
--style mallets_pep --bpm 126, straight). The output is the same every time (fixed random seed).

usage: python3 tools/compose_music.py <out music.bin> [preview.wav] [--style acoustic|mallets|mallets_pep|handheld]
       [--bpm N] [--swing S] [--repeats N]
  music.bin  the app's music format (tools/make_music.py: "PPMU" header, intro played once, loop forever),
             32728 Hz (the 3DS DSP's rate), 16-bit, mono (MONO below)
  preview    intro + loop (repeated --repeats times, default 1), stereo, to listen to on the PC
  --style    the instrument set (default acoustic):
               acoustic  plucked nylon guitar arpeggios, soft flute lead, upright bass, brushes + shaker
               mallets   marimba arpeggios + soft electric piano, vibraphone lead, music box (B), warm bass
               handheld  soft square lead, triangle bass, gentle pulse arpeggios, soft noise drums
  --bpm      tempo (default 116)

Form: intro 4 bars, then the loop: A (8 bars, lead), B (8 bars, second melody), A' (8 bars, rising ending back to A).
The loop is rendered twice and the second pass is kept, so the echo / room / note tails at its end run over into
its start exactly as they will when it repeats - the seam cannot be heard.

v1 (2026-11-16, 144 bpm, synth set) was "a little too hyper", owner did not like the instruments -> v2: slower,
arpeggios instead of off-beat stabs, long bass notes, sparse soft percussion, three acoustic-leaning sets.
"""
import argparse
import struct

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, lfilter

SR = 32728
MONO = True  # the app plays it as a quiet background: mono halves the size (stereo only in the preview)
rng = np.random.default_rng(20261116)
BEAT = 60.0 / 116
SWING = 0.5


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


def tvec(n):
    return np.arange(n) / SR


def env(n, a, d, s, r, length):
    """ADSR in seconds; length = how long the key is held"""
    t = tvec(n)
    e = np.where(t < a, t / max(a, 1e-4), s + (1 - s) * np.exp(-(t - a) / max(d, 1e-4)))
    rel = t >= length
    e[rel] = e[rel] * np.exp(-(t[rel] - length) / max(r, 1e-4))
    return e


def lowpass(x, f, order=2):
    b, a = butter(order, min(f, SR / 2 - 100) / (SR / 2))
    return lfilter(b, a, x)


def noise(n):
    return rng.standard_normal(n)


# --- instruments (each returns a mono note) -----------------------------------------------------------------------
def guitar(m, length, vel):  # plucked nylon string (Karplus-Strong, worked block by block)
    n = int((length + 1.2) * SR)
    p = max(2, int(round(SR / hz(m))))
    buf = lowpass(noise(p), 3500, 1)
    out = np.zeros(n)
    loss = 0.996 if m < 60 else 0.993
    i = 0
    while i < n:
        k = min(p, n - i)
        out[i:i + k] = buf[:k]
        buf = loss * 0.5 * (buf + np.roll(buf, 1))
        i += p
    damp = np.ones(n)
    held = int(length * SR)
    damp[held:] = np.exp(-tvec(n - held) / 0.12)
    return vel * 0.9 * out * damp


def flute(m, length, vel):  # soft flute: near-sine, a little breath, late vibrato
    n = int((length + 0.3) * SR)
    t = tvec(n)
    vib = 1 + 0.003 * np.sin(2 * np.pi * 5.0 * t) * np.clip((t - 0.25) / 0.3, 0, 1)
    ph = 2 * np.pi * hz(m) * np.cumsum(vib) / SR
    x = np.sin(ph) + 0.12 * np.sin(2 * ph) + 0.05 * np.sin(3 * ph)
    breath = lowpass(noise(n), 2500) * 0.05 * np.exp(-t / 0.15)
    return vel * 0.7 * (x + breath) * env(n, 0.07, 0.4, 0.85, 0.15, length)


def upright(m, length, vel):  # upright-ish bass: round, plucked, fades while held
    n = int((length + 0.3) * SR)
    t = tvec(n)
    f = hz(m)
    x = np.sin(2 * np.pi * f * t) + 0.35 * np.sin(4 * np.pi * f * t) * np.exp(-t / 0.15)
    thump = 0.2 * np.sin(2 * np.pi * f * 0.5 * t) * np.exp(-t / 0.03)
    return vel * (x + thump) * env(n, 0.006, 0.7, 0.35, 0.1, length)


def marimba(m, length, vel):  # wooden bar: fundamental + the ~4x partial, both fade fast
    n = int(1.0 * SR)
    t = tvec(n)
    f = hz(m)
    x = np.sin(2 * np.pi * f * t) * np.exp(-t / 0.35) + 0.25 * np.sin(2 * np.pi * f * 3.93 * t) * np.exp(-t / 0.05)
    return vel * x * np.clip(t / 0.002, 0, 1)


def vibes(m, length, vel):  # vibraphone: metal bar with a slow tremolo
    n = int((length + 1.5) * SR)
    t = tvec(n)
    f = hz(m)
    x = np.sin(2 * np.pi * f * t) + 0.12 * np.sin(2 * np.pi * f * 4 * t) * np.exp(-t / 0.15)
    trem = 1 - 0.25 * (0.5 + 0.5 * np.sin(2 * np.pi * 4.5 * t))
    return vel * 0.8 * x * trem * env(n, 0.003, 1.2, 0.0, 0.4, length + 0.6)


def musicbox(m, length, vel):  # small tines: bright, short, a touch inharmonic
    n = int(1.4 * SR)
    t = tvec(n)
    f = hz(m)
    x = np.sin(2 * np.pi * f * t) * np.exp(-t / 0.6) + 0.3 * np.sin(2 * np.pi * f * 5.4 * t) * np.exp(-t / 0.08)
    return vel * 0.7 * x * np.clip(t / 0.001, 0, 1)


def rhodes(m, length, vel):  # soft electric piano: sine with a little bark, no bright tine
    n = int((length + 0.8) * SR)
    t = tvec(n)
    f = hz(m)
    x = np.sin(2 * np.pi * f * t + 0.5 * np.exp(-t / 0.3) * np.sin(2 * np.pi * f * t))
    return vel * x * env(n, 0.01, 0.9, 0.4, 0.4, length)


def warmbass(m, length, vel):  # warm sine bass
    n = int((length + 0.2) * SR)
    t = tvec(n)
    f = hz(m)
    x = np.sin(2 * np.pi * f * t) + 0.2 * np.sin(4 * np.pi * f * t)
    return vel * x * env(n, 0.01, 0.5, 0.6, 0.1, length)


def pulse(m, length, vel, duty=0.25):  # band-limited pulse wave, softened
    n = int((length + 0.15) * SR)
    t = tvec(n)
    vib = 1 + 0.0035 * np.sin(2 * np.pi * 5.5 * t) * np.clip((t - 0.2) / 0.2, 0, 1)
    ph = 2 * np.pi * hz(m) * np.cumsum(vib) / SR
    x = np.zeros(n)
    for k in range(1, 12):
        x += np.sin(np.pi * k * duty) / k * np.cos(k * ph)
    return vel * 0.6 * lowpass(x, 4000) * env(n, 0.01, 0.3, 0.7, 0.08, length)


def triangle(m, length, vel):  # triangle bass
    n = int((length + 0.08) * SR)
    t = tvec(n)
    ph = 2 * np.pi * hz(m) * t
    x = sum(((-1) ** ((k - 1) // 2)) * np.sin(k * ph) / (k * k) for k in (1, 3, 5, 7))
    return vel * 1.1 * x * env(n, 0.004, 0.3, 0.8, 0.04, length)


def pad(m, length, vel):  # slow, slightly detuned soft pad (very quiet)
    n = int((length + 0.8) * SR)
    t = tvec(n)
    x = np.zeros(n)
    for det in (-0.05, 0.0, 0.06):
        f = hz(m + det)
        x += np.sin(2 * np.pi * f * t) + 0.3 * np.sin(4 * np.pi * f * t)
    return vel * 0.1 * x * env(n, 0.8, 1.0, 0.8, 0.8, length)


def kick(vel):
    n = int(0.3 * SR)
    t = tvec(n)
    f = 48 + 50 * np.exp(-t / 0.03)
    return vel * np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.1)


def brush(vel):  # a soft swish
    n = int(0.22 * SR)
    t = tvec(n)
    b, a = butter(2, [1500 / (SR / 2), 6000 / (SR / 2)], btype='band')
    shape = np.clip(t / 0.03, 0, 1) * np.exp(-t / 0.08)
    return vel * 0.35 * lfilter(b, a, noise(n)) * shape


def rim(vel):  # a woody click
    n = int(0.08 * SR)
    t = tvec(n)
    return vel * 0.5 * (np.sin(2 * np.pi * 820 * t) + 0.5 * np.sin(2 * np.pi * 1650 * t)) * np.exp(-t / 0.015)


def shaker(vel):
    n = int(0.07 * SR)
    t = tvec(n)
    b, a = butter(2, [4500 / (SR / 2), 9000 / (SR / 2)], btype='band')
    return vel * 0.12 * lfilter(b, a, noise(n)) * np.clip(t / 0.01, 0, 1) * np.exp(-t / 0.025)


def chipnoise(vel, length=0.05):  # soft noise drum of the handheld set
    n = int(length * SR)
    t = tvec(n)
    return vel * 0.15 * lowpass(noise(n), 5000) * np.exp(-t / (length / 3))


STYLES = {
    # chords: instrument for the arpeggio (+ optional sustained chord instrument), lead A, lead B, bass, drums
    'acoustic': dict(arp=guitar, arp_vel=.30, hold=None, lead=flute, lead_vel=.22, leadB=vibes, leadB_vel=.22,
                     bass=upright, bass_vel=.55, drums='brush', pad=True),
    'mallets': dict(arp=marimba, arp_vel=.26, hold=rhodes, hold_vel=.07, lead=vibes, lead_vel=.25,
                    leadB=musicbox, leadB_vel=.24, bass=warmbass, bass_vel=.5, drums='rim', pad=False),
    # v3 (owner: "I like the mallets ... it lacks the calmness but pep" of the X/Y Wi-Fi song): the same soft sounds,
    # pep from the rhythm - a bouncing bass, soft off-beat chords, a gentle groove (and --swing)
    'mallets_pep': dict(arp=marimba, arp_vel=.22, hold=None, stab=rhodes, stab_vel=.075, lead=vibes, lead_vel=.25,
                        leadB=musicbox, leadB_vel=.24, bass=warmbass, bass_vel=.5, bass_style='bounce',
                        drums='groove', pad=False),
    'handheld': dict(arp=lambda m, ln, v: pulse(m, ln, v, 0.125), arp_vel=.10, hold=None,
                     lead=lambda m, ln, v: pulse(m, ln, v, 0.25), lead_vel=.16,
                     leadB=lambda m, ln, v: pulse(m, ln, v, 0.5), leadB_vel=.13, bass=triangle, bass_vel=.5,
                     drums='chip', pad=False),
}

# --- the score ----------------------------------------------------------------------------------------------------
# chords: (bass root, voicing - low to high)
CH = {
    'F': (41, [53, 57, 60, 64, 67]), 'Dm7': (38, [50, 57, 60, 62, 65]), 'Bbmaj7': (46, [46 + 12, 62, 65, 69]),
    'C7sus': (48, [48 + 12, 65, 67, 70]), 'C7': (48, [48 + 12, 64, 67, 70]), 'Am7': (45, [57, 60, 64, 67]),
    'Gm7': (43, [55, 58, 62, 65]), 'C': (48, [55, 60, 64, 67]),
}
INTRO = ['F', 'Dm7', 'Bbmaj7', 'C7sus']
A = ['F', 'Dm7', 'Bbmaj7', 'C7sus', 'F', 'Am7', 'Gm7', 'C7']
B = ['Bbmaj7', 'C', 'Am7', 'Dm7', 'Gm7', 'Am7', 'Bbmaj7', 'C7sus']
A2 = ['F', 'Dm7', 'Bbmaj7', 'C7sus', 'F', 'Am7', 'Gm7', 'C7']
# melodies: (beat in the section, length in beats, MIDI note)
MEL_A = [(0, .5, 72), (.5, .5, 69), (1, 1, 72), (2, .5, 74), (2.5, 1.5, 77),
         (4, .5, 76), (4.5, .5, 74), (5, 1, 72), (6, 1, 69), (7, 1, 74),
         (8, 1.5, 74), (9.5, .5, 72), (10, 1, 70), (11, 1, 69),
         (12, 1, 67), (13, .5, 70), (13.5, .5, 69), (14, 2, 67),
         (16, .5, 72), (16.5, .5, 69), (17, 1, 72), (18, .5, 74), (18.5, 1.5, 77),
         (20, .5, 79), (20.5, .5, 77), (21, 1, 76), (22, 1, 72), (23, 1, 76),
         (24, 1, 74), (25, .5, 72), (25.5, .5, 70), (26, 1, 74), (27, 1, 72),
         (28, 1.5, 70), (29.5, .5, 69), (30, 2, 67)]
MEL_B = [(0, 2, 69), (2, 1, 70), (3, 1, 72), (4, 3, 72), (7, 1, 74),
         (8, 2, 76), (10, 1, 74), (11, 1, 72), (12, 3, 69), (15, 1, 72),
         (16, 2, 70), (18, 1, 72), (19, 1, 74), (20, 2, 76), (22, 1, 77), (23, 1, 79),
         (24, 3, 77), (27, 1, 76), (28, 2, 74), (30, 2, 72)]
MEL_A2 = MEL_A[:-3] + [(28, 1, 70), (29, 1, 72), (30, 1, 74), (31, 1, 76)]  # rising back into A
ARP = [0, 2, 3, 4, 3, 2, 1, 2]  # which voicing note on each 8th of a bar (rolls up and back down)


class Mix:
    def __init__(self, beats):
        self.n = int((beats * BEAT + 3.0) * SR)
        self.tracks = {k: np.zeros(self.n) for k in ('arp', 'hold', 'bass', 'lead', 'leadB', 'pad', 'drums')}

    def add(self, track, beat, sig):
        # swing (v3): an 8th note on the "and" comes a little late (SWING = 0.5 straight, 0.58 a light swing)
        whole = np.floor(beat + 1e-9)
        if abs(beat - whole - 0.5) < 1e-6:
            beat = whole + SWING
        i = int(round(beat * BEAT * SR))
        j = min(self.n, i + len(sig))
        if i < self.n:
            self.tracks[track][i:j] += sig[:j - i]


def section(mx, st, start, chords, melody, which, intro=False):
    for bar, name in enumerate(chords):
        b0 = start + 4 * bar
        root, voice = CH[name]
        # arpeggio: gentle 8ths over the chord, softer on the off-beats
        for e, k in enumerate(ARP):
            m = voice[k % len(voice)]
            mx.add('arp', b0 + e / 2, st['arp'](m, 0.9 * BEAT, st['arp_vel'] * (1.0 if e % 2 == 0 else 0.7)))
        if st['hold']:
            for m in voice[1:]:
                mx.add('hold', b0, st['hold'](m, 3.8 * BEAT, st['hold_vel']))
        if st.get('stab'):  # soft chords on the "and" of 2 and 4
            for off in (1.5, 3.5):
                for m in voice[1:]:
                    mx.add('hold', b0 + off, st['stab'](m, 0.35 * BEAT, st['stab_vel']))
        if st['pad'] and which == 'B':
            for m in voice[1:]:
                mx.add('pad', b0, pad(m, 4 * BEAT, 1.0))
        # bass: long notes - root, then the fifth; a walking step into the next chord on even bars
        if not (intro and bar < 1) and st.get('bass_style') == 'bounce':
            # short notes that move: root, octave, root on the "and", fifth, a step up into the next chord
            v = st['bass_vel']
            for off, ln, m, g in ((0, .45, root, 1.0), (1, .3, root + 12, .6), (1.5, .4, root, .8),
                                  (2.5, .45, root + 7, .85), (3, .3, root + 12, .55), (3.5, .4, root + 9, .7)):
                mx.add('bass', b0 + off, st['bass'](m, ln * BEAT, v * g))
        elif not (intro and bar < 1):
            mx.add('bass', b0, st['bass'](root, 1.9 * BEAT, st['bass_vel']))
            mx.add('bass', b0 + 2, st['bass'](root + 7, 1.4 * BEAT, st['bass_vel'] * .8))
            if bar % 2 == 1:
                mx.add('bass', b0 + 3.5, st['bass'](root + 5, .45 * BEAT, st['bass_vel'] * .6))
        # percussion: sparse and soft
        if intro and bar < 2:
            continue
        d = st['drums']
        if d == 'brush':
            mx.add('drums', b0, kick(.35))
            mx.add('drums', b0 + 2, kick(.25))
            for off in (1, 3):
                mx.add('drums', b0 + off, brush(.6))
            for e in range(8):
                mx.add('drums', b0 + e / 2, shaker(.5 if e % 2 else .3))
        elif d == 'groove':  # soft but moving: kick 1 and 3 (+ a ghost before 3), rim on the backbeat, shaker 8ths
            mx.add('drums', b0, kick(.4))
            mx.add('drums', b0 + 2, kick(.3))
            mx.add('drums', b0 + 1.5, kick(.15))
            for off in (1, 3):
                mx.add('drums', b0 + off, rim(.35))
            for e in range(8):
                mx.add('drums', b0 + e / 2, shaker(.55 if e % 2 else .3))
        elif d == 'rim':
            mx.add('drums', b0, kick(.3))
            mx.add('drums', b0 + 2.5, kick(.2))
            mx.add('drums', b0 + 3, rim(.25))
            for e in range(8):
                mx.add('drums', b0 + e / 2, shaker(.4 if e % 2 else .25))
        else:
            mx.add('drums', b0, kick(.3))
            for off in (1, 3):
                mx.add('drums', b0 + off, chipnoise(.6, 0.09))
            for e in range(1, 8, 2):
                mx.add('drums', b0 + e / 2, chipnoise(.3))
    for beat, ln, m in melody:
        if which == 'B':
            mx.add('leadB', start + beat, st['leadB'](m, ln * BEAT * .95, st['leadB_vel']))
        else:
            mx.add('lead', start + beat, st['lead'](m, ln * BEAT * .92, st['lead_vel']))


def echo(x, beats, fb, mixamt):
    d = int(beats * BEAT * SR)
    y = x.copy()
    for k in range(1, 5):
        y[d * k:] += x[:-d * k] * (fb ** k) * mixamt
    return y


def room(x, seconds=1.2, amount=0.2):
    n = int(seconds * SR)
    ir = noise(n) * np.exp(-tvec(n) / (seconds / 5))
    ir = lowpass(ir, 4500, 1)
    ir /= np.sqrt((ir ** 2).sum())
    wet = np.fft.irfft(np.fft.rfft(x, len(x) + n) * np.fft.rfft(ir, len(x) + n))[:len(x)]
    return x + amount * wet


def highpass(x, f, order=2):
    b, a = butter(order, f / (SR / 2), btype='high')
    return lfilter(b, a, x)


def render(style, speaker=True):
    st = STYLES[style]
    intro_beats, loop_beats = 4 * len(INTRO), 4 * (len(A) + len(B) + len(A2))
    mx = Mix(intro_beats + 2 * loop_beats)
    section(mx, st, 0, INTRO, [], 'A', intro=True)
    global rng
    for p in range(2):  # the loop twice: the second pass is the one kept (its start carries the first's tails)
        rng = np.random.default_rng(20261116)  # both passes identical (plucks / noise drums), so the seam matches
        s = intro_beats + p * loop_beats
        section(mx, st, s, A, MEL_A, 'A')
        section(mx, st, s + 4 * len(A), B, MEL_B, 'B')
        section(mx, st, s + 4 * (len(A) + len(B)), A2, MEL_A2, 'A')
    tr = mx.tracks
    tr['lead'] = echo(tr['lead'], 1.5, 0.3, 0.35)
    tr['leadB'] = echo(tr['leadB'], 1.5, 0.3, 0.35)
    pan = {'arp': -0.3, 'hold': 0.2, 'bass': 0.0, 'lead': 0.1, 'leadB': 0.25, 'pad': -0.1, 'drums': 0.0}
    gain = {'arp': 1.0, 'hold': 1.0, 'bass': 1.0, 'lead': 1.0, 'leadB': 1.0, 'pad': 0.7, 'drums': 0.6}
    if speaker:
        gain['bass'] = 0.75  # v0.76 0.6 ("very tinny") -> v0.77 0.75; see the speaker master below
        # v0.77 (owner: "if you could muffle whatever the high pitch instrument is"): the music box (lead B, the
        # B section's melody) through a low-pass and 30 % quieter
        tr['leadB'] = lowpass(tr['leadB'], 1500)
        gain['leadB'] = 0.7
    L = np.zeros(mx.n)
    R = np.zeros(mx.n)
    for k, x in tr.items():
        L += x * gain[k] * (1 - pan[k])
        R += x * gain[k] * (1 + pan[k])
    L, R = room(L), room(R)
    # warm master: gentle low-pass, then a soft compressor (tanh) for an even level
    L, R = lowpass(L, 7000), lowpass(R, 7000)
    drive = 1.5
    if speaker:
        # v0.76 (owner: "kind of quiet and peaks the speakers even at low volume"): 84 % of the mix was below 400 Hz,
        # which the 3DS's small speakers cannot play - they distorted on it while little was heard. Speaker master:
        # the bass under ~200 Hz goes (the ear still hears the bass line from its overtones), the bass track is
        # quieter, and the softer peaks let the level come up (a little more drive into the soft limiter).
        # v0.77 (owner on v0.76: "very distorted and very tinny ... like if I turned my car speaker up all the way"):
        # the cut moved down to 120 Hz and the limiter drive back to v0.73's 1.5 (v0.76: 200 Hz, drive 2.0)
        L, R = highpass(L, 120), highpass(R, 120)
    pk = max(np.abs(L).max(), np.abs(R).max())
    L, R = np.tanh(drive * L / pk) / np.tanh(drive), np.tanh(drive * R / pk) / np.tanh(drive)
    i0 = int(round(intro_beats * BEAT * SR))
    i1 = int(round((intro_beats + loop_beats) * BEAT * SR))
    i2 = int(round((intro_beats + 2 * loop_beats) * BEAT * SR))
    stx = np.stack([L, R], axis=1)
    intro, loop = stx[:i0], stx[i1:i2]
    peak = max(np.abs(intro).max(), np.abs(loop).max())
    return intro / peak * 0.85, loop / peak * 0.85


def main():
    global BEAT, SWING
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('preview', nargs='?')
    # defaults = the version in the app (v0.73, owner: "Let's go with v3 straight"): mallets_pep, 126 bpm, straight;
    # since v0.76 with the speaker master (--full-bass = the v0.73 file)
    ap.add_argument('--style', default='mallets_pep', choices=sorted(STYLES))
    ap.add_argument('--bpm', type=float, default=126.0)
    ap.add_argument('--repeats', type=int, default=1)
    ap.add_argument('--swing', type=float, default=0.5, help='0.5 = straight, 0.58 = a light swing')
    ap.add_argument('--full-bass', action='store_true',
                    help='the v0.73 master (all the bass; for headphones) instead of the speaker master (v0.76)')
    a = ap.parse_args()
    BEAT = 60.0 / a.bpm
    SWING = a.swing
    intro, loop = render(a.style, speaker=not a.full_bass)
    body = np.concatenate([intro, loop])
    if MONO:
        body = body.mean(axis=1, keepdims=True)
    pcm = np.clip(np.round(body * 32767), -32768, 32767).astype('<i2')
    with open(a.out, 'wb') as f:
        f.write(b'PPMU' + struct.pack('<IHHII', SR, pcm.shape[1], 0, len(intro), len(loop)))
        f.write(pcm.tobytes())
    print('wrote %s (%s, %.0f bpm, swing %.2f): intro %.2f s, loop %.2f s, %d channel(s), %d bytes' % (
        a.out, a.style, a.bpm, a.swing, len(intro) / SR, len(loop) / SR, pcm.shape[1], 16 + pcm.nbytes))
    if a.preview:
        prev = np.concatenate([intro] + [loop] * a.repeats)
        wavfile.write(a.preview, SR, np.clip(np.round(prev * 32767), -32768, 32767).astype(np.int16))
        print('preview %s: %.1f s' % (a.preview, len(prev) / SR))


if __name__ == '__main__':
    main()
