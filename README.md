# <img src="OpenRGBEffectsPlugin.png" width="48" height="48" style="vertical-align: middle;"/> OpenRGB Effects Plugin

[![Pipeline Status](https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin/badges/master/pipeline.svg)](https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin/-/commits/master)

Synchronize your [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) lighting with a wide variety of customizable effects including audio visualizations, screen mirroring (Ambilight), OpenGL shaders, and many more.

## Features

* Apply custom, software-driven effects to any OpenRGB device that supports Direct mode
* Choose from a wide variety of effects
* Synchronize your lights to your music
* Synchronize your lights to match what's on screen
* Use OpenGL shaders such as those from [ShaderToy](https://shadertoy.com) as RGB effects

## Website

* Check out our website at [openrgb.org/plugin_effects](https://openrgb.org/plugin_effects.html)

## Supported Devices

* Supports any OpenRGB device that has Direct mode

## Installation

  * Pre-built binaries are available for the following platforms:
    * Windows
    * Linux (.so, .deb, and .rpm)
    * MacOS
  * Released versions are available to download on [OpenRGB.org](https://openrgb.org/plugin_effects.html).
  * Experimental (aka Pipeline) versions are available to download on [OpenRGB.org](https://openrgb.org/plugin_effects.html#pl).
  * The pre-built .so binaries are intended to be used with the OpenRGB AppImage builds and may not be compatible with OpenRGB installed from other sources.
  * Arch users can also install from the AUR for the [release](https://aur.archlinux.org/packages/openrgb-plugin-effects/) or [pipeline](https://aur.archlinux.org/packages/openrgb-plugin-effects-git/) version.
 
## Effects List

```
├── Ambient
├── AudioParty
├── AudioSine
├── AudioStar
├── AudioSync
├── AudioVisualizer
├── Bloom
├── BouncingBall
├── Breathing
├── BreathingCircle
├── Bubbles
├── ColorWheelEffect
├── Comet
├── CrossingBeams
├── CustomGradientWave
├── CustomMarquee
├── DoubleRotatingRainbow
├── Fill
├── FractalMotion
├── GifPlayer
├── GradientWave
├── Hypnotoad
├── Layers
├── Lightning
├── Marquee
├── Mask
├── Mosaic
├── MotionPoint
├── MotionPoints
├── MovingPanes
├── NoiseMap
├── RadialRainbow
├── Rain
├── RainbowWave
├── RotatingBeam
├── RotatingRainbow
├── Sequence
├── Shaders
├── SmoothBlink
├── SpectrumCycling
├── Spiral
├── Stack
├── StarryNight
├── Sunrise
├── Swap
├── SwirlCircles
├── SwirlCirclesAudio
├── Visor
└── Wavy
```

## SDK support

This plugin is supported by the OpenRGB SDK, see [SDK docs](./Documentation/SDK.md) for more details.

## Common Issues

### My CPU usage is really high

There are a few effects that cause this:

* Audio effects
* Ambient

Both of the audio effects will spike in CPU usage when opening an [S/PDIF device](https://en.wikipedia.org/wiki/S/PDIF) for reasons we still don't know. This can be fixed by switching off of the spdif device or stopping the effect.

The very nature of ambient is to do massive calulation to get the average or most common color on screen. Scaling uses a lot of CPU so once you have selected your portion of the screen it is recommended that you hide the preview.

## Contributing

Check out the [compilation instructions](./Documentation/Compilation.md) to build this project locally.

Please read the [contributing guide](./Documentation/CONTRIBUTING.md) if you want to add effects or bring new features.
