# Seer · UI design

The window carries the synth's full panel on one screen, dressed as analog hardware: wooden end
cheeks, near-black panels with cream silkscreen frames and lettering, physical knobs, red LEDs,
and a grain over everything. The field of light from Keys' Nocturne look still moves behind the
panels, dimmer.

![The window](images/seer-window.jpg)

The first draft, for comparison: [images/seer-panel-draft.png](images/seer-panel-draft.png).

- Interactive draft: [`mockup/panel.html`](mockup/panel.html), published privately at
  https://claude.ai/artifact/X6Bv36bAcJHL1hLWBSCnFA
- Every control and its parameter ID: [sound-design.md](sound-design.md)

Teletone Audio's instruments set the mood (few colours, reactive artwork, evocative lettering).
The artwork, names and layout are our own, and the hardware's panel graphics are not copied.

## Window

Design size 1280 × 812, key strip included; scaled as a whole from 70 % to 150 % by dragging the
window's corner.

```
header   ☰ menu · status chip (voices, quality, bend range) · ◀ preset ▶
title    S E E R
row 1    LFO · Poly Mod · Oscillator 1 · Oscillator 2 · Drift (slop, pan) · Mixer
row 2    Low-pass (large cutoff knob) · High-pass · Filter envelope · Amp envelope
row 3    Voice (unison) · Glide · Aftertouch · Output (distortion, level) · Effect A · Effect B
keys     pitch wheel · mod wheel · master volume · master tune · 49 keys, C2–C6
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

Voice count (6, 8, 10, 12), quality (2× or 4× oversampling), pitch-wheel range, transpose, Save
preset, Open the preset folder. Master volume and tune are the two knobs beside the wheels.

## Header

The status line shows a MIDI lamp, the MIDI inputs (Standalone), how many voices are sounding,
the quality and the bend range. The preset readout steps with its arrows; clicking the name opens
the list, grouped into Leads, Basses, Pads, Keys and Yours.

## How it is built

| Part | Class | File |
|---|---|---|
| Colours, fonts, knob / fader / LED / dropdown drawing | `SeerLookAndFeel` | `source/ui/SeerLookAndFeel.*` |
| A section plate that lays out its own controls | `Panel` | `source/ui/Controls.*` |
| Knob, fader, switch, dropdown | `KnobControl`, `FaderControl`, `LedToggle`, `ChoiceControl` | `source/ui/Controls.*` |
| Preset readout, status line, wheels, keys | `PresetDisplay`, `StatusChip`, `WheelControl`, `SeerKeyboard` | `source/ui/Controls.*` |
| The light, the grain and the wood | `LightField`, `Overlay` | `source/ui/Controls.*` |
| The window: which control goes on which panel | `SeerEditor` | `source/PluginEditor.*` |

The light is painted into a 160 × 100 image 15 times a second and stretched over the window.
Panels and the grain are kept as images, so a frame of light costs a few copies. With the window
open and nothing playing, the Standalone app uses about 7 % of one core.
