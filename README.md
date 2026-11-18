# Mono Reverb

The REVERB machine of the Elektron Monomachine (FX-REVERB) as a CLAP effect
for Bitwig (macOS, universal binary).

The DSP is a fixed-point C port of the machine in Monomachine OS 1.32B,
written from its DSP56300 code and checked sample for sample against that
code running in an emulator. No firmware code is in this repository: the
machine's tables are generated from formulas at start-up (see Accuracy).

## Controls

| Parameter | Range | On the Monomachine | What it does |
|-----------|-------|--------------------|--------------|
| Decay     | 0–127 | DEC  | The tail's decay time (RT60 of the delay lines): 0.4 s at 0, 1.3 s at 64, 4 s at 127, in 64 steps. Higher settings also diffuse a little more. |
| Damp      | 0–127 | DAMP | Darkens the tail with a low-pass in each delay line. On the longest line it is about 19 kHz at 2, 3 kHz at 32 and 106 Hz at 127; the shorter lines' are higher. 0 is no damping. |
| Gate      | 0–127 | GATE | Shuts the reverb once the input has not got louder for the hold time: 0 at 0, 0.37 s at 64, 1.44 s at 126. 127 is off. |
| Mix       | 0–127 | MIX  | 0: dry only. 127: the reverb, with 1/128 of the dry signal left, as on the unit. |
| Highpass  | 0–127 | HP   | One-pole high-pass on the reverb: 10 Hz at 0, 447 Hz at 64, 18.8 kHz at 127. |
| Lowpass   | 0–127 | LP   | One-pole low-pass on the reverb, on the same scale. |
| Input     | 0–127 | INP  | Gain before the reverb, 4 × (Input / 128)²: 64 is unity, 127 is +11.9 dB, 0 is silence. |

The defaults are the machine's. Its seventh knob (shown as "-" on the unit)
does nothing and is left out. All parameters can be automated and modulated
in Bitwig.

## How it behaves

This is what the firmware does; the plug-in keeps all of it.

- **Structure.** The two input channels are summed, then go through five
  allpass diffusers (47 to 223 samples) into six delay lines (2663, 2447,
  1811, 1621, 1399 and 613 samples). Each line is read through its damping
  low-pass and fed back into the others through sums and differences, with
  1/6 of the diffused input added. Two of the lines are read with a slow
  ±4-sample wobble (5.4 Hz), which keeps the tail from ringing. The left and
  right outputs are two different sums of the lines.
- **Decay.** Each line's feedback is set so that it loses 60 dB in the Decay
  time, for its own length. Decay also raises the diffusers' gains slightly.
- **Gate.** A level follower on the diffused input holds the gate open; when
  the input has not risen for the hold time, the reverb fades out (×0.99 a
  sample) and opens again within about 2 ms when the input rises.
- **Filters.** High-pass, then low-pass, on the reverb only; each output
  trails its input by a sample.
- **Knobs.** Parameter changes reach the machine the way the Monomachine's
  CPU sends a knob: every 384 samples the value moves a quarter of the way to
  where you set it. Modulation is added on top, unsmoothed, every 48 samples,
  as the unit's LFOs are.

## Differences from the hardware

- **The machine alone.** On the unit, REVERB is a machine on a track: it
  takes its input from INP A/B, a bus or the neighbouring track, and its
  output goes on through the track's amp, filter, EQ and delay. The plug-in
  is the machine alone: the host's audio goes in where the machine's input
  is, and the machine's output comes out.
- **Start.** For its first 256 blocks (93 ms) after it is assigned to a
  track, the machine keeps the reverb silent. The plug-in runs those blocks
  on silence whenever it starts or is reset, so it is ready to play at once.
- **Other sample rates.** The machine runs at 44.1 kHz, the unit's rate. At
  other rates the plug-in resamples in and out (windowed sinc). The dry part
  of the mix goes through the resampling too, so it is limited to 20.9 kHz,
  and the output is no longer bit-exact. The reverb's times stay the same in
  seconds.
- **Levels.** 0 dBFS in the plug-in is full scale at the machine's input.

Latency is 16 samples at 44.1 kHz (one 16-sample block), 108 at 48 kHz and
213 at 96 kHz. It is reported to the host, and at the other rates the
resampling is shifted so that the reported latency is exact to the sample.

## Accuracy

The reference is Monomachine OS 1.32B running in the DSP56300 emulator of
[Monomodule](https://github.com/shnolk/monomodule), with the machine's
controls built by Monomodule's model of the Monomachine's CPU. It has not
been compared with a physical Monomachine.

| Check | Result |
|-------|--------|
| Core vs. firmware: from the machine's start, knob moves through the CPU model, raw control words anywhere from 0 to $7FFFFF, inputs up to full scale, bursts and silence, 402,000 blocks (the gate shut in about a quarter of them) | every output sample identical; every state word identical after every block, and all eleven delay buffers every 16 blocks |
| Built plug-in code vs. firmware: knob moves through the CPU model and modulation (7,887 moves, 2,033 of them modulation, in 240,000 blocks after the start) | every output sample identical |
| Tables vs. firmware memory | identical, every entry the machine reads |

The machine reads four tables. The decay gains are
0.7071 × 10^(−3 d / (44100 T)) for each line's delay d, with
T = 0.4 s × 10^(r/63); the formula is one LSB off the firmware in two of
its 384 entries, which are corrected in code. The damping coefficients are
p^d with p = 1 − 2^−23 − 0.000025 r, exactly. The filter coefficients are the
table the Monomachine's track filters and delay use (corrected in three
entries, as in [Mono Delay](https://github.com/nerazin/mono-delay-clap)), and
the wobble reads the firmware's sine table, which the boot code's recurrence
reproduces exactly.

`tests/test_core.c` pins this without the firmware. Its expected hashes were
produced by feeding the same stimulus into the firmware's machine, which the
core matched.

## Build and install

```bash
./build_macos.sh
```

```bash
cp -R "build/Mono Reverb.clap" ~/Library/Audio/Plug-Ins/CLAP/
```

The build script needs Xcode's command-line tools.

## Tests

```bash
cc -std=c99 -O2 -Wno-unused-function tests/test_core.c -lm -o tests/test_core && tests/test_core
```

```bash
xcrun clang -std=c11 -O2 -Wno-unused-function -Wno-missing-field-initializers -I vendor/clap/include tests/clap_test.c -o tests/clap_test && tests/clap_test "build/Mono Reverb.clap"
```

`test_core` checks the tables and 12,000 blocks of output against the
firmware-derived hashes. `clap_test` loads the built plug-in and checks:

- at 44.1 kHz, output equal to the core's after its 256 silent blocks;
- with Mix at 0, the input arrives after the reported latency at 44.1, 48
  and 96 kHz;
- an impulse's tail decaying at the rate Decay sets, the same at 44.1 and
  48 kHz (measured RT60 1.20 s at Decay 64, whose lines are tuned for 1.29 s);
- state and the parameter text.

The plug-in also passes clap-trap's `validate` (at 44.1, 48 and 96 kHz) and
`state --roundtrip`.

## Source

- `source/mono_reverb.h`: the machine (C99 and libm).
- `source/mono_reverb_clap.c`: the CLAP plug-in around it.
- `vendor/clap`: CLAP 1.2 headers (MIT).

## License

GPL-3.0-only. The DSP is an independent reimplementation of the
Monomachine's REVERB machine, derived from analysis of Elektron's firmware;
no firmware is included. Monomachine and Elektron are trademarks of Elektron
Music Machines; this project is not affiliated with Elektron.
