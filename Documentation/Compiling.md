# Compiling

This document details the process to compile the OpenRGB Effects Plugin from source on supported operating systems.
In this steps, we assume you have followed the [OpenRGB Compilation Document](https://gitlab.com/CalcProgrammer1/OpenRGB/-/blob/master/Documentation/Compiling.md) already.

First, clone the repo locally:

```
git clone https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin.git --recursive
```

or via ssh:

```
git clone git@gitlab.com:OpenRGBDevelopers/OpenRGBEffectsPlugin.git --recursive
```

## Dependencies

Install `libopenal` and `pipewire`

### Fedora

```
sudo dnf install openal-soft pipewire-pulseaudio
```

### OSX

```
brew install openal-soft pipewire
```

## Windows

stub

## Linux

```
mkdir build
cd build
qmake ../OpenRGBEffectsPlugin.pro && make -j$(nproc)
```

This creates a file `build/libOpenRGBEffectsPlugin.so`, which can be imported into OpenRGB.

## MacOS

stub


