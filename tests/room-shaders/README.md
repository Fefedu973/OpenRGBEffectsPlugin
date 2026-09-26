# Shader image math tests

In a VS2022 x64 Developer PowerShell:

```powershell
python tests/room-shaders/run.py --qt C:/Qt/6.8.3/msvc2022_64
```

Checks working-size bounds, 800×600 source sampling, source image immutability,
black at zero brightness, neutral colors, tint/temperature and CPU conversion of
60 synthetic frames. This does not create an OpenGL context, run an effect or
access hardware. The printed timing measures only the CPU adjustment helper,
not capture/render/output fps. A complete plugin build checks the actual Qt slots
and renderer integration separately.
