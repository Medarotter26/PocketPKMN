#!/usr/bin/env python3
"""PocketPKMN background music, take 2 ("Pocket Stroll", rebuilt from scratch for the 3DS speakers).

Every sound is synthesised here (no samples, no recordings). Same score as tools/compose_music.py (PocketPKMN's own
chords and melodies, F major), new instruments, new arrangement and a new, click-free renderer:
  * every note gets a raised-cosine attack (>= 4 ms) and its buffer is sized from its decay, then the last 15 ms are
    a raised-cosine release that ends at exactly 0 - nothing is cut off while it still rings;
  * filters / echo / room run over whole tracks, never per note (except the shaker, filtered before its envelope);
  * no limiter, no tanh, no clipping: the mix keeps its headroom and the peak is scaled to -3 dBFS;
  * the loop is rendered three times; the middle pass is kept, so its start carries the tails of the loop's end;
    the intro's last bar is the loop's last bar (same chord, bass, run up into A) and is cross-faded into the
    rendered loop end, so intro -> loop is continuous as well;
  * for small speakers: the bass line is voiced an octave up with overtones, everything is high-passed at 150 Hz
    and low-passed around 4.5-5.5 kHz, the melody tops out at G5 (784 Hz).

usage: python3 tools/compose_song2.py <out music.bin> [preview.wav] [--variant warm|bright]
  music.bin   the app's format: b'PPMU' + <IHHII (rate, channels, 0, intro samples, loop samples) + int16 mono PCM
  preview     intro + the loop twice, mono, to listen to on the PC
At the end a click check and level / spectrum figures are printed.
"""
import argparse
import struct
import zlib

import numpy as np
from scipy.io import wavfile
from scipy.ndimage import uniform_filter1d
from scipy.signal import butter, fftconvolve, sosfilt

SR = 32728
ATTACK = 0.004     # s, raised cosine
RELEASE = 0.015    # s, raised cosine, ends at exactly 0


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


def finish(x, attack=ATTACK, release=RELEASE):
    """raised-cosine fade in and out; the first and the last sample are exactly 0"""
    n = len(x)
    a, r = max(2, int(attack * SR)), max(2, int(release * SR))
    e = np.ones(n)
    e[:a] = 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    e[n - r:] *= 0.5 + 0.5 * np.cos(np.pi * (np.arange(r) + 1) / r)
    y = x * e
    assert y[0] == 0 and y[-1] == 0
    return y


def decay(t, tau, damp_at, damp_tau):
    """natural exponential decay, plus a faster damping once the key is let go"""
    e = np.exp(-t / tau)
    k = t > damp_at
    e[k] *= np.exp(-(t[k] - damp_at) / damp_tau)
    return e


def dur(tau, damp_at, damp_tau):
    """seconds until the slowest partial is ~-60 dB, plus room for the release"""
    return min(6.9 * tau, damp_at + 6.9 * damp_tau) + RELEASE + 0.005


def tvec(seconds):
    return np.arange(int(seconds * SR)) / SR


# --- instruments: (midi note, held seconds, variant params) -> mono note, starting and ending at 0 -------------------
def marimba(m, hold, p):
    f = hz(m)
    tau = float(np.clip(0.5 * (220 / f) ** 0.6, 0.15, 0.6))
    t = tvec(dur(tau, hold + 0.12, 0.07))
    x = np.sin(2 * np.pi * f * t) * decay(t, tau, hold + 0.12, 0.07)
    x += p['mar_ot'] * np.sin(2 * np.pi * f * 3.93 * t) * np.exp(-t / 0.045)          # the woody "tock"
    x += 0.10 * np.sin(2 * np.pi * f * 2 * t) * decay(t, tau * 0.5, hold + 0.12, 0.07)  # body for small speakers
    return finish(x, 0.004)


def vibes(m, hold, p):
    f = hz(m)
    t = tvec(dur(1.1, hold + 0.25, 0.14))
    body = decay(t, 1.1, hold + 0.25, 0.14)
    x = np.sin(2 * np.pi * f * t) + 0.16 * np.sin(2 * np.pi * f * 2 * t) * np.exp(-t / 0.5)
    x = x * body + p['vib_ot'] * np.sin(2 * np.pi * f * 4 * t) * np.exp(-t / 0.10)
    trem = 1 - p['trem'] * (0.5 - 0.5 * np.cos(2 * np.pi * 4.6 * t))
    return finish(x * trem, 0.005)


def epiano(m, hold, p):
    """soft electric piano: FM sine, the brightness fades fast; no tine ping"""
    f = hz(m)
    t = tvec(dur(1.0, hold, 0.06))
    idx = p['ep_index'] * np.exp(-t / 0.22)
    x = np.sin(2 * np.pi * f * t + idx * np.sin(2 * np.pi * f * t))
    return finish(x * decay(t, 1.0, hold, 0.06), 0.006)


def bass(m, hold, p):
    """plucked synth bass, voiced an octave up, with overtones (heard on small speakers); upper ones fade first"""
    f = hz(m)
    t = tvec(dur(0.5, hold, 0.035))
    x = np.zeros(len(t))
    for k in range(1, 12):
        fk = f * k
        if fk > 3200:
            break
        amp = (0.9 if k == 1 else 1.0) / k ** 1.3 / (1 + (fk / p['bass_fc']) ** 2)
        x += amp * np.sin(2 * np.pi * fk * t) * decay(t, 0.5 / (1 + 0.35 * (k - 1)), hold, 0.035)
    return finish(x, 0.004)


def pad(m, hold, p):
    """very soft held chord tone: two slightly detuned sines + an octave, slow swell, slow fade"""
    f = hz(m)
    t = tvec(hold + 0.35 * 6.9 + RELEASE + 0.005)
    x = sum(np.sin(2 * np.pi * f * (1 + d) * t + ph) + 0.25 * np.sin(4 * np.pi * f * (1 + d) * t + ph)
            for d, ph in ((-0.0015, 0.0), (0.0017, 1.3)))
    e = np.clip(t / 0.35, 0, 1) ** 2 * decay(t, 1e9, hold, 0.35)
    return finish(x * e, 0.01)


def knock(_m, _hold, p):
    """soft kick for small speakers: a short pitched body around 120-190 Hz with two overtones"""
    t = tvec(0.40)
    fr = 118 + 70 * np.exp(-t / 0.02)
    ph = 2 * np.pi * np.cumsum(fr) / SR
    x = (np.sin(ph) + 0.45 * np.sin(2 * ph) + 0.15 * np.sin(3 * ph)) * np.exp(-t / 0.06)
    return finish(x, 0.003)


def wood(_m, _hold, p):
    """a soft wood block"""
    t = tvec(0.16)
    x = (np.sin(2 * np.pi * 740 * t) * np.exp(-t / 0.022) + 0.3 * np.sin(2 * np.pi * 1930 * t) * np.exp(-t / 0.010))
    return finish(x, 0.003)


_SHAKER_SOS = None


def shaker(m, _hold, p):
    """band-passed noise (filtered BEFORE its envelope, so the filter tail is inside the note); m picks 1 of 4"""
    global _SHAKER_SOS
    if _SHAKER_SOS is None:
        _SHAKER_SOS = butter(2, [2400, 5200], btype='band', fs=SR, output='sos')
    rng = np.random.default_rng(777 + int(m) % 4)
    t = tvec(0.18)
    nz = sosfilt(_SHAKER_SOS, rng.standard_normal(len(t) + 400))[400:]
    x = nz * np.clip(t / 0.012, 0, 1) * np.exp(-t / 0.028)
    return finish(x, 0.006)


_cache = {}


def note(inst, m, hold, p):
    key = (inst.__name__, m, round(hold, 4))
    if key not in _cache:
        _cache[key] = inst(m, hold, p)
    return _cache[key]


# --- variants ----------------------------------------------------------------------------------------------------
VARIANTS = {
    # warm: 120 bpm, darker mallets, a soft electric piano for the B melody, few drums
    'warm': dict(bpm=120, mar_ot=0.16, vib_ot=0.06, trem=0.18, ep_index=0.6, bass_fc=900, lp=4500,
                 leadB='epiano', shaker8=False, pad_B_only=False,
                 g=dict(arp=.20, comp=.055, lead=.30, leadB=.30, bass=.30, knock=.18, wood=.10, shaker=.10,
                        pad=.035)),
    # bright: 126 bpm (the owner's tempo), a little more mallet "tock", vibes on both melodies, shaker 8ths
    'bright': dict(bpm=126, mar_ot=0.32, vib_ot=0.12, trem=0.22, ep_index=0.9, bass_fc=1800, lp=5500,
                   leadB='vibes', shaker8=True, pad_B_only=True,
                   g=dict(arp=.21, comp=.06, lead=.30, leadB=.27, bass=.30, knock=.17, wood=.12, shaker=.12,
                          pad=.035)),
}
# v0.79 (owner, after the two previews: "The drum and synth can go but I like warm"): warm without the drums (knock,
# wood block, shaker) and without the soft held chords (the pad); the peak goes to -1 dBFS (was -3) because the mix
# lost those parts' level - the app adds the rest with its music volume. This is the app's song.
# Then (owner: "I meant remove the instrument that comes in around 2 seconds not the other one"): the soft held chords
# stay; what goes is the short electric-piano chords on the off-beats ("comp", from the intro's 2nd bar = 2 s).
# And then ("The instrument is still there"): the other part that enters at 2 s is the bass (pitched an octave up,
# with overtones - it can sound like a synth) - it goes too.
# Then ("It's gone, lower the keyboard volume slightly though"): the electric piano (lead B, the B section's melody)
# 0.30 -> 0.24 (about -2 dB).
VARIANTS['warm_nodrums'] = dict(VARIANTS['warm'], peak_db=-1.0,
                                g=dict(VARIANTS['warm']['g'], knock=0.0, wood=0.0, shaker=0.0, comp=0.0, bass=0.0,
                                       leadB=0.24, lead=0.25))
# v0.82 (owner: "lower the main instrument a little"): the vibraphone melody (lead A) 0.30 -> 0.25 (about -1.6 dB).

# --- the score (PocketPKMN's own chords and melodies, from tools/compose_music.py) -------------------------------
# chord: (bass note - an octave above a normal bass line, voicing for arpeggio / chords, low to high)
CH = {
    'F': (53, [53, 57, 60, 64, 67]), 'Dm7': (50, [53, 57, 60, 62, 65]), 'Bbmaj7': (58, [58, 62, 65, 69]),
    'C7sus': (60, [58, 65, 67, 70]), 'C7': (60, [58, 64, 67, 70]), 'Am7': (57, [57, 60, 64, 67]),
    'Gm7': (55, [55, 58, 62, 65]), 'C': (60, [55, 60, 64, 67]),
}
INTRO = ['F', 'Dm7', 'Bbmaj7', 'C7']
A = ['F', 'Dm7', 'Bbmaj7', 'C7sus', 'F', 'Am7', 'Gm7', 'C7']
B = ['Bbmaj7', 'C', 'Am7', 'Dm7', 'Gm7', 'Am7', 'Bbmaj7', 'C7sus']
A2 = A
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
RUN = [(28, 1, 70), (29, 1, 72), (30, 1, 74), (31, 1, 76)]   # rising back into A
MEL_A2 = MEL_A[:-3] + RUN
MEL_INTRO = [(b - 16, ln, m) for b, ln, m in RUN]          # the intro's last bar = the loop's last bar
ARP = [0, 2, 3, 4, 3, 2, 1, 2]
# bass, per bar: (beat, length in beats, interval above the root, gain) - root, octave pop, root, fifth, octave, step
BOUNCE = [(0, .45, 0, 1.0), (1, .3, 12, .5), (1.5, .4, 0, .8), (2.5, .45, 7, .85), (3, .3, 12, .45), (3.5, .4, 9, .65)]


def jitter(*key):
    """deterministic small humanising from a key (the same in every loop pass): (seconds offset, gain)"""
    r = np.random.default_rng(zlib.crc32(repr(key).encode())).random(2)
    return (r[0] - 0.5) * 0.006, 1 + (r[1] - 0.5) * 0.1


class Mix:
    def __init__(self, n):
        self.n = n
        self.tr = {}

    def add(self, track, pos, sig, gain):
        x = self.tr.setdefault(track, np.zeros(self.n))
        i = max(0, int(round(pos)))  # (a note humanised to before the very start is moved to sample 0, not cut)
        j = min(self.n, i + len(sig))
        x[i:j] += gain * sig[:j - i]


def section(mx, v, spb, base, chords, melody, which, tag, intro=False):
    """base: sample index of the section start; spb: samples per beat"""
    P, g = v, v['g']
    leadB = vibes if v['leadB'] == 'vibes' else epiano
    bt = spb / SR  # seconds per beat

    def at(beat, *key):
        dt, gg = jitter(tag, beat, *key)
        return base + beat * spb + dt * SR, gg

    for bar, name in enumerate(chords):
        b0 = 4 * bar
        root, voice = CH[name]
        for e, k in enumerate(ARP):
            pos, gg = at(b0 + e / 2, 'arp')
            mx.add('arp', pos, note(marimba, voice[k % len(voice)], 0.45 * bt, P),
                   g['arp'] * gg * (1.0 if e % 2 == 0 else 0.72))
        if g['pad'] and (which == 'B' or not P['pad_B_only']):
            for i, m in enumerate(voice[1:]):
                pos, gg = at(b0, 'pad', i)
                mx.add('pad', pos, note(pad, m, 3.6 * bt, P), g['pad'])
        if not (intro and bar < 1):
            for off in (1.5, 3.5):  # soft chords on the "and" of 2 and 4
                for i, m in enumerate(voice[1:]):
                    pos, gg = at(b0 + off, 'comp', i)
                    mx.add('comp', pos + i * 0.004 * SR, note(epiano, m, 0.35 * bt, P), g['comp'] * gg)
            for off, ln, iv, gb in BOUNCE:
                pos, gg = at(b0 + off, 'bass')
                mx.add('bass', pos, note(bass, root + iv, ln * bt, P), g['bass'] * gb * gg)
        if intro and bar < 2:
            continue
        for off, gk in ((0, 1.0), (1.5, .45), (2, .75)):
            pos, gg = at(b0 + off, 'knock')
            mx.add('drums', pos, note(knock, 0, 0, P), g['knock'] * gk * gg)
        for off in (1, 3):
            pos, gg = at(b0 + off, 'wood')
            mx.add('drums', pos, note(wood, 0, 0, P), g['wood'] * gg)
        for e in range(8):
            if e % 2 == 0 and not P['shaker8']:
                continue
            pos, gg = at(b0 + e / 2, 'shaker')
            mx.add('drums', pos, note(shaker, e, 0, P), g['shaker'] * gg * (1.0 if e % 2 else 0.55))
    for beat, ln, m in melody:
        pos, gg = at(beat, 'mel', m)
        if which == 'B':
            mx.add('leadB', pos, note(leadB, m, ln * bt * .9, P), g['leadB'] * gg)
        else:
            mx.add('lead', pos, note(vibes, m, ln * bt * .9, P), g['lead'] * gg)


def echo(x, d, fb, amt):
    y = x.copy()
    for k in range(1, 5):
        y[d * k:] += x[:-d * k] * (fb ** k) * amt
    return y


def room(x, seconds=1.0, amount=0.16):
    rng = np.random.default_rng(4242)
    t = tvec(seconds)
    ir = rng.standard_normal(len(t)) * np.exp(-t / (seconds / 5)) * np.clip(t / 0.01, 0, 1)
    ir = sosfilt(butter(1, 3500, fs=SR, output='sos'), ir)
    ir = finish(ir, 0.003, 0.05)
    ir /= np.sqrt((ir ** 2).sum())
    return x + amount * fftconvolve(x, ir)[:len(x)]


def render(name):
    v = VARIANTS[name]
    spb = 60.0 / v['bpm'] * SR
    bars_loop = len(A) + len(B) + len(A2)
    n_intro = int(round(4 * len(INTRO) * spb))
    n_loop = int(round(4 * bars_loop * spb))
    n_bar = int(round(4 * spb))
    mx = Mix(n_intro + 3 * n_loop + 3 * SR)
    section(mx, v, spb, 0, INTRO, MEL_INTRO, 'A', 'intro', intro=True)
    for p in range(3):  # 3 identical passes; the middle one is kept
        s = n_intro + p * n_loop
        section(mx, v, spb, s, A, MEL_A, 'A', 'A')
        section(mx, v, spb, s + 4 * len(A) * spb, B, MEL_B, 'B', 'B')
        section(mx, v, spb, s + 4 * (len(A) + len(B)) * spb, A2, MEL_A2, 'A', 'A2')
    tr = mx.tr
    d = int(round(1.5 * spb))
    tr['lead'] = echo(tr['lead'], d, 0.3, 0.3)
    tr['leadB'] = echo(tr['leadB'], d, 0.3, 0.3)
    x = sum(tr.values())
    x = room(x)
    x = sosfilt(butter(2, 150, btype='high', fs=SR, output='sos'), x)
    x = sosfilt(butter(2, v['lp'], fs=SR, output='sos'), x)
    s1, s2 = n_intro + n_loop, n_intro + 2 * n_loop
    loop = x[s1:s2].copy()
    intro = x[:n_intro].copy()
    # intro's last bar -> the loop's rendered last bar (same notes), raised-cosine cross-fade
    w = 0.5 - 0.5 * np.cos(np.pi * np.arange(n_bar) / (n_bar - 1))
    intro[-n_bar:] = intro[-n_bar:] * (1 - w) + x[s2 - n_bar:s2] * w
    seam_err = np.abs(x[s2:s2 + SR] - x[s1:s1 + SR]).max()  # pass 3's start vs pass 2's start (what really repeats)
    pk = max(np.abs(intro).max(), np.abs(loop).max())
    k = 10 ** (v.get('peak_db', -3.0) / 20) / pk
    return intro * k, loop * k, seam_err * k, v


# --- checks ------------------------------------------------------------------------------------------------------
def click_check(stream):
    hp = sosfilt(butter(4, 3500, btype='high', fs=SR, output='sos'), stream)
    w = int(0.02 * SR)
    s2 = uniform_filter1d(hp * hp, w) * w
    local = np.sqrt(np.maximum(s2 - hp * hp, 0) / (w - 1))  # the window's RMS without the sample itself
    sus = np.flatnonzero(np.abs(hp) > 8 * local + 10 ** (-80 / 20))
    clusters = 0 if len(sus) == 0 else 1 + int((np.diff(sus) > int(0.005 * SR)).sum())
    return clusters, sus[:10], float(np.max(np.abs(hp) / (local + 1e-9)))


def joint(a, b, ref):
    """a ends, b starts: step and slope change at the joint, against the 99.9th percentile within ref"""
    d1, d2 = np.abs(np.diff(ref)), np.abs(np.diff(ref, 2))
    step = abs(b[0] - a[-1])
    curv = abs((b[0] - a[-1]) - (a[-1] - a[-2])) + abs((b[1] - b[0]) - (b[0] - a[-1]))
    return step, np.percentile(d1, 99.9), curv / 2, np.percentile(d2, 99.9)


def bands(x):
    p = np.abs(np.fft.rfft(x)) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    tot = p.sum()
    edges = [(0, 150), (150, 400), (400, 2000), (2000, 6000), (6000, SR)]
    return ['%d-%s Hz %.1f %%' % (lo, hi if hi < SR else 'up', 100 * p[(f >= lo) & (f < hi)].sum() / tot)
            for lo, hi in edges]


def db(x):
    return 20 * np.log10(max(x, 1e-12))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('preview', nargs='?')
    ap.add_argument('--variant', default='warm_nodrums', choices=sorted(VARIANTS))  # v0.79: the app's song
    a = ap.parse_args()
    intro, loop, seam_err, v = render(a.variant)
    q = lambda y: np.clip(np.round(y * 32767), -32768, 32767).astype('<i2')
    pi, pl = q(intro), q(loop)
    with open(a.out, 'wb') as f:
        f.write(b'PPMU' + struct.pack('<IHHII', SR, 1, 0, len(pi), len(pl)))
        f.write(pi.tobytes())
        f.write(pl.tobytes())
    print('wrote %s (%s, %d bpm, straight): intro %.2f s, loop %.2f s, mono, %d bytes' % (
        a.out, a.variant, v['bpm'], len(pi) / SR, len(pl) / SR, 16 + pi.nbytes + pl.nbytes))
    stream = np.concatenate([pi, pl, pl]).astype(float) / 32768
    if a.preview:
        wavfile.write(a.preview, SR, np.concatenate([pi, pl, pl]).astype(np.int16))
        print('preview %s: %.1f s (intro + loop twice)' % (a.preview, len(stream) / SR))
    body = np.concatenate([pi, pl]).astype(float) / 32768
    print('level: peak %.2f dBFS, RMS %.2f dBFS (intro+loop); loop RMS %.2f dBFS' % (
        db(np.abs(body).max()), db(np.sqrt(np.mean(body ** 2))), db(np.sqrt(np.mean((pl / 32768.) ** 2)))))
    print('energy: ' + ', '.join(bands(body)))
    n, where, worst = click_check(stream)
    print('click check (3.5 kHz high-pass, |x| > 8x local RMS): %d suspected clicks; worst ratio %.1f%s' % (
        n, worst, '' if n == 0 else ' at %s s' % ', '.join('%.3f' % (i / SR) for i in where)))
    lf, lp = pl.astype(float) / 32768, pi.astype(float) / 32768
    for label, x0, x1 in (('loop end -> loop start', lf, lf), ('intro end -> loop start', lp, lf)):
        step, p1, curv, p2 = joint(x0, x1, lf)
        print('  %s: step %.5f (99.9th pct within the loop %.5f), slope change %.5f (99.9th pct %.5f) -> %s' % (
            label, step, p1, curv, p2, 'OK' if step <= p1 and curv <= p2 else 'SUSPECT'))
    print('  loop seam vs what really repeats (pass 3 start - pass 2 start): max diff %.2e' % seam_err)


if __name__ == '__main__':
    main()
