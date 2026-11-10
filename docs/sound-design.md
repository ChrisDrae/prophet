# Seer · sound design

What the engine models, every parameter with its ID and range, and where each number comes from.
"Seer" is a working name.

Sources:

- **[M]** the Prophet-6 Operation Manual (v1.0), including its NRPN table (Appendix C). This fixes
  the control set, the switch options and each control's resolution.
- **[A]** an assumption of mine. The manual gives no times, frequencies or curves, so these start
  from typical values for this family of instruments and get tuned in M2
  (`docs/calibration.md` will record each one).

## Signal path

```
per voice (6):
  OSC 1 ─┐
  SUB ───┤
  OSC 2 ─┼─ MIXER ─► LOW-PASS 24 dB ─► HIGH-PASS 12 dB ─► VCA ─► PAN
  NOISE ─┘
all voices ─► DISTORTION (stereo) ─► FX A ─► FX B ─► VOICE VOLUME ─► MASTER VOLUME
```

- The manual says the two filters together act as a band-pass, and that the sound goes "through
  the filters" into the VCA. It doesn't state which filter comes first. **[A]** low-pass first.
  The order only matters when a filter is driven hard; it's one constant in `Voice.cpp`.
- Effects run in series, A then B. Reverbs exist only in slot B. **[M]**
- With FX off, or a mix at 0, the effect is skipped exactly (the hardware uses a true bypass). **[M]**

## Parameters

IDs are final once released: hosts store automation by ID.
"Steps" is the hardware's resolution **[M]**; Seer's parameters are continuous unless marked.

### Oscillators

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `osc1Freq` `osc2Freq` | Frequency | 0–60 semitones, stepped | 24 | **[M]** 61 steps; 9 octaves, 16 Hz–8 kHz, together with transpose |
| `osc2Fine` | Fine | −50…+50 cents | +3 | **[M]** a quarter-tone each way |
| `osc1Shape` `osc2Shape` | Shape | 0–1 | 0.5 | **[M]** triangle → sawtooth → pulse, continuous. 0 = triangle, 0.5 = sawtooth, 1 = pulse |
| `osc1Pw` `osc2Pw` | Pulse width | 0–1 | 0.5 | **[M]** square at centre, very narrow at both ends. **[A]** 2 %–98 % |
| `osc1Sync` | Sync | off / on | off | **[M]** osc 1 restarts whenever osc 2 starts a cycle |
| `osc2LowFreq` | Low freq | off / on | off | **[M]** osc 2 becomes an LFO. **[A]** 8 octaves down |
| `osc2Kbd` | Keyboard | off / on | on | **[M]** off: osc 2 ignores the keys |

### Mixer

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `mixOsc1` `mixOsc2` | Osc 1, Osc 2 | 0–1 | 1, 0.8 | **[M]** high levels are allowed to overload the filter |
| `mixSub` | Sub octave | 0–1 | 0 | **[M]** a triangle one octave below osc 1 |
| `mixNoise` | Noise | 0–1 | 0 | **[M]** white |

### Filters

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `lpCutoff` `hpCutoff` | Cutoff | 0–164 | 164, 0 | **[M]** 165 steps. **[A]** semitones upward from 10 Hz, limited to 22 kHz |
| `lpRes` `hpRes` | Resonance | 0–1 | 0 | **[M]** the low-pass self-oscillates at high settings |
| `lpEnv` `hpEnv` | Env amount | −1…+1 | 0 | **[M]** bipolar. **[A]** ±96 semitones at full |
| `lpVel` `hpVel` | Velocity | off / on | off | **[M]** velocity scales the env amount |
| `lpKey` `hpKey` | Keyboard | Off / Half / Full | Off | **[M]** Full: self-oscillation plays in tune. Half: in quarter-tones |

### Envelopes

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `fEnvA` `aEnvA` | Attack | 0–1 | 0 | **[A]** 1 ms–10 s, exponential |
| `fEnvD` `aEnvD` | Decay | 0–1 | 0.4 | **[A]** 3 ms–20 s, exponential |
| `fEnvS` `aEnvS` | Sustain | 0–1 | 0.5, 1 | |
| `fEnvR` `aEnvR` | Release | 0–1 | 0.3 | **[A]** 3 ms–20 s, exponential |
| `aEnvAmt` | Amp env amount | 0–1 | 1 | **[M]** at 0 the VCA only opens through LFO → amp |
| `aEnvVel` | Amp velocity | off / on | on | **[M]** velocity scales the amount |

### LFO

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `lfoRate` | Frequency | 0–1 | 0.55 | **[A]** 0.02 Hz–500 Hz, exponential (the manual: "into the audible range") |
| `lfoAmt` | Initial amount | 0–1 | 0 | **[M]** at 0, the mod wheel alone brings the LFO in |
| `lfoShape` | Shape | Triangle / Saw / Reverse saw / Square / Random | Triangle | **[M]** triangle and random are bipolar; the other three only go positive. Random at full rate becomes noise |
| `lfoFreq1` `lfoFreq2` `lfoPw` `lfoAmp` `lfoLp` `lfoHp` | Destinations | off / on | off | **[M]** |

The LFO restarts when a key is pressed with no other key held. **[M]**

### Poly Mod

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `pmEnv` | Filter env amount | −1…+1 | 0 | **[M]** bipolar |
| `pmOsc2` | Osc 2 amount | −1…+1 | 0 | **[M]** bipolar, audio rate; follows osc 2's shape |
| `pmFreq1` `pmShape1` `pmPw1` `pmLp` `pmHp` | Destinations | off / on | off | **[M]** |

### Aftertouch

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `atAmt` | Amount | −1…+1 | 0 | **[M]** bipolar, channel pressure |
| `atFreq1` `atFreq2` `atLfo` `atAmp` `atLp` `atHp` | Destinations | off / on | off | **[M]** |

### Voice

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `unison` | Unison | off / on | off | **[M]** monophonic, voices stacked |
| `unisonVoices` | Voices | 1–6 | 1 | **[M]** chord memory is not built |
| `unisonDetune` | Detune | 0–1 | 0.2 | Beyond the manual, which detunes a stack with Slop only. **[A]** up to ±35 cents across the stack |
| `keyMode` | Key mode | Low / High / Last / Low retrig / High retrig / Last retrig | Last | **[M]** |
| `glideOn` | Glide | off / on | off | **[M]** |
| `glideRate` | Glide rate | 0–1 | 0.3 | **[A]** up to 4 s per octave (rate modes) or 4 s (time modes) |
| `glideMode` | Glide mode | Fixed rate / Fixed rate legato / Fixed time / Fixed time legato | Fixed rate | **[M]** |
| `slop` | Slop | 0–1 | 0.08 | **[M]** "subtle to wildly out of tune". **[A]** up to ±50 cents |
| `panSpread` | Pan spread | 0–1 | 0.25 | **[M]** every other voice goes the opposite way |

### Output

| ID | Control | Range | Default | Notes |
|---|---|---|---|---|
| `dist` | Distortion | 0–1 | 0 | **[M]** stereo, after the voices |
| `fxOn` | Effects | off / on | on | **[M]** one switch for both slots |
| `fxAType` | FX A type | Off / BBD delay / Digital delay / Chorus / Phaser 1 / Phaser 2 | Off | **[M]** |
| `fxBType` | FX B type | the above + Hall / Room / Plate / Spring | Off | **[M]** |
| `fxAMix` `fxBMix` | Mix | 0–1 | 0.3 | **[M]** fully dry to fully wet |
| `fxAP1` `fxBP1` | Parameter 1 | 0–1 | 0.5 | **[M]** delay time (to 1 s) · rate · reverb time · spring decay |
| `fxAP2` `fxBP2` | Parameter 2 | 0–1 | 0.4 | **[M]** feedback · depth · early reflections · spring tone |
| `voiceVol` | Voice volume | 0–1 | 0.75 | **[M]** per preset, for matching levels |

### Player settings (presets leave these alone)

| ID | Control | Range | Default |
|---|---|---|---|
| `masterVol` | Master volume | −60…+6 dB | 0 dB |
| `masterTune` | Master tune | −50…+50 cents | 0 |
| `transpose` | Transpose | −2…+2 octaves | 0 |
| `bendRange` | Pitch wheel range | 0–12 semitones | 2 |
| `velCurve` `atCurve` | Velocity / aftertouch response | Curve 0–3 | 0 |
| `voices` | Voice count | 6 / 8 / 10 / 12 | 6 |
| `quality` | Oversampling | 2× / 4× | 2× |

### MIDI

- Notes, velocity, pitch wheel (14-bit, smoothed), mod wheel (CC 1), sustain (CC 64), channel
  aftertouch, all-notes-off.
- The hardware's CC map **[M]** is built in, so a controller set up for the real instrument works:
  CC 102 low-pass cutoff, 103 resonance, 106 high-pass cutoff, 9 distortion, 8 sub level, 67 / 75
  osc frequency, 70 / 78 shape, 43–46 amp ADSR, 50–53 filter ADSR, 47 / 54 env amounts, 65 glide.
  In Ableton, the usual parameter mapping works as well.

## The model

### Oversampling

Voices and their sum run at 2× the host rate, then pass a half-band low-pass (95 taps: flat to
19.7 kHz, −80 dB above 24.4 kHz at a 44.1 kHz host rate) and drop back down. A 4× mode comes
with milestone 2. The plugin reports the delay this adds (about 28 samples) to the host. Reasons: the filter's nonlinearity,
audio-rate Poly Mod and sync all make partials above the audible band, which would otherwise
fold back as inharmonic noise.

### Oscillators

- One sawtooth core per oscillator, phase in double precision, free-running (a new note doesn't
  reset the phase).
- **Shape**: 0–0.5 fades triangle into sawtooth, 0.5–1 fades sawtooth into pulse. Both sides are
  derived from the same phase, so the in-between shapes stay in tune and in phase.
- **Band-limiting**: each jump (sawtooth reset, both pulse edges, a sync restart) is replaced by a
  band-limited step from a table (a windowed sinc, 8 zero crossings each side); each corner of
  the triangle by the matching band-limited ramp. Jump positions are found to sub-sample
  accuracy. Measured: aliasing below −80 dB for every shape and for sync, with a 3.7 kHz note.
- **Sync**: when osc 2 wraps, osc 1's phase restarts at the same sub-sample position, and the
  jump this causes in each of osc 1's shapes gets its own correction.
- **Sub**: a triangle at half of osc 1's frequency, from a divide-by-two of osc 1's phase.
- Pulse width limits are 2 % and 98 % **[A]**. The pulse has its DC offset removed.
- **Still to come (M2) [A]**: the sawtooth's finite reset time and AC coupling, if calibration
  shows they matter; and **Slop**: per oscillator, a slow random walk (below 1 Hz) plus a small
  offset drawn at each note-on, both scaled by the knob.

### Low-pass

Four one-pole stages in a feedback loop (the layout of the classic 4-pole filters this one
descends from), solved without a delay in the loop:

- a soft saturation at the input (anti-aliased), so the filter thickens and compresses when the
  mixer drives it;
- a saturation in the feedback path, which holds self-oscillation at a steady level;
- the loop equation, saturation included, solved exactly per sample by Newton's method;
- the four stages are linear. A first version saturated every stage, as the circuit does, but
  that pulled the self-oscillation up to three semitones flat, so it was dropped: now it is
  within a tenth of a cent from 110 Hz to 5 kHz;
- resonance reaches self-oscillation at about 93 % of the knob; with Keyboard = Full it plays in tune;
- the passband level drops as resonance rises. **[A]** about −6 dB at full, less than a plain ladder.

### High-pass

A 2-pole state-variable filter, also solved without delay, with resonance up to a strong peak
(Q 0.7 to 12, no self-oscillation **[A]**) and a soft limit on the resonant state.

### Envelopes

RC curves, as an analog envelope makes them: the attack aims at 1.3 and is cut off at 1.0 (a
slightly convex rise), decay and release fall exponentially. A retrigger continues from the
current level. Velocity scales the amount, not the times.

### Voices

- 6 voices (8–12 optional, M2). A new note reuses the voice already on that note, otherwise the
  voice that has been silent longest, otherwise steals the oldest released voice, then the oldest
  held one.
- Each voice has fixed small offsets, seeded: cutoff ±2 %, envelope times ±3 %. They're part of
  why chords on an analog poly sound wide. Oscillators run free: each voice's start at its own
  point in the cycle.
- Velocity **[A]**: with a filter's Velocity on, its Env Amount is scaled from 20 % (softest) to
  100 %. With Amp Velocity on, the level runs from 12 % to 100 %.
- Unison stacks 1–6 voices on one note, spread by Detune and by Pan spread.
- Glide: per voice, in pitch (semitones), in the four modes above.

### Modulation rates

Per sample at the oversampled rate: envelopes, the oscillators, both filters (their cutoff
slides in a straight line between control ticks), and from M2 osc 2 → Poly Mod.
Per control tick (8 oversampled samples, about 0.1 ms): knob positions (glided over about
12 ms), pitch, the LFO and everything it modulates, the wheels.

LFO depths at full amount **[A]**: pitch ±12 semitones (the amount is squared, so the lower
half of the wheel gives fine vibrato), cutoff ±48 semitones, pulse width ±45 %.

### Distortion and effects (M4)

- Distortion: an asymmetric soft clipper per channel with antiderivative anti-aliasing, a
  tone-shaping pair of filters around it, and level compensation so the knob adds grit more than
  volume.
- BBD delay: a modulated delay with companding and a low-pass that closes as the delay time
  grows. Digital delay: clean, up to 1 s. Chorus: two BBD-style lines. Phasers: 6 all-pass stages,
  high and low feedback. Reverbs: hall, room, plate (time, early reflections) and spring (decay,
  tone).

## What is not modeled

The sequencer, the arpeggiator, chord memory, alternative tunings, LFO and delay clock sync, MPE.
