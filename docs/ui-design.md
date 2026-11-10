# Seer · UI design

The window carries the synth's full panel on one screen, dressed as analog hardware: wooden end
cheeks, near-black panels with cream silkscreen frames and lettering, physical knobs, red LEDs,
and a grain over everything. The field of light from Keys' Nocturne look still moves behind the
panels, dimmer.

![The panel draft](images/seer-panel-draft.png)

- Interactive draft: [`mockup/panel.html`](mockup/panel.html), published privately at
  https://claude.ai/artifact/X6Bv36bAcJHL1hLWBSCnFA
- Every control and its parameter ID: [sound-design.md](sound-design.md)

Teletone Audio's instruments set the mood (few colours, reactive artwork, evocative lettering).
The artwork, names and layout are our own, and the hardware's panel graphics are not copied.

## Window

Design size 1280 × 700, plus 120 for the key strip; scaled as a whole from 70 % to 150 %.

```
header   ☰ menu · status chip (voices, quality, bend range) · ◀ preset ▶
title    S E E R
row 1    LFO · Poly Mod · Oscillator 1 · Oscillator 2 · Drift (slop, pan) · Mixer
row 2    Low-pass (large cutoff knob) · High-pass · Filter envelope · Amp envelope
row 3    Voice (unison) · Glide · Aftertouch · Output (distortion, level) · Effect A · Effect B
keys     pitch wheel · mod wheel · 49 keys, C2–C6
```

The rows follow the signal: sources, then filters and envelopes, then how it plays and where it
goes.

## Controls

| Control | Looks like | Used for |
|---|---|---|
| Knob, 42 px | dark skirt and cap with a drop shadow, one cream pointer line, an 11-mark printed scale | most parameters |
| Large knob, 88 px | the same with a brushed-silver cap | low-pass cutoff only |
| Fader | recessed slot with a printed scale, a dark cap with a cream line | the two ADSRs |
| LED | red lamp in a chrome bezel, lit when on | switches and modulation destinations |
| Dropdown | recessed window, mono capitals, caption below | shapes, modes, effect types |
| Preset display | red LED-style readout between ◀ ▶ | preset name |

Interaction is the same as in Keys: drag up or down, Shift for fine steps, double-click to reset,
scroll wheel, arrow keys. A control's caption turns into its value while the mouse is over it; the printed scales don't
light up, so that readout and the pointer are how a value is read.
A panel's frame takes its section's colour while one of its controls moves.
Dropdowns sit above the knobs in their panel, so each row fits on one line.

## Section colours and the light field

| Section | Hue | In the field |
|---|---|---|
| Oscillators, mixer | amber `#e8a33d`, orange `#e8763a`, cream `#f4e3c3` | warm light, brighter with the mixer levels |
| Low-pass | red `#e8263f` | brighter as the cutoff opens |
| High-pass, effects | blue `#4f8fe0`, `#2f66c8` | blue haze |
| LFO, aftertouch | violet `#7a56ec` | violet light; the field moves faster with LFO amount and slop |
| Poly Mod, glide | magenta `#c23a94`, `#b02a84` | grows with resonance and distortion |
| Notes | cream | a glow at the key's position that fades with the note |

## Menu (☰)

Voice count, oversampling quality, pitch-wheel range, master tune, transpose, velocity and
aftertouch response, still background, and the MIDI monitor from Keys for checking what the
controller sends.

## Reused from Keys

`KeysLookAndFeel`, `Tile`, `KnobColumn`, `FaderColumn`, `CaptionedChoice`, `ParamToggle`,
`PresetBox`, `MenuButton`, `LibraryChip`, `ColourField`, `KeysStrip`, `MenuPanel`, and the
editor's scaling and computer-keyboard code. New: the denser layout, the wheels, the section hues.
