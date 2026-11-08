# SuperTuxKart for PlayStation Vita

This is a port of SuperTuxKart 1.5 to the PlayStation Vita, based on the port by
[Rinnegatamante](https://github.com/Rinnegatamante/stk-code). It renders with
[vitaGL](https://github.com/Rinnegatamante/vitaGL) and spreads the physics and
the scene work over the three cores of the Vita.

All the game modes work offline: races, time trials, follow the leader, battles
(3 strikes and free-for-all), soccer and egg hunts. Capture the flag is network
only in SuperTuxKart. With 6 karts most tracks run at about 60 FPS; the heaviest
(Black Forest, Cocoa Temple, Candela City) at 53-58.

## Renderers

- **Legacy** (default): irrlicht's fixed pipeline over OpenGL ES 2, the fastest.
- **Shader-based**: the OpenGL 3 renderer of the PC version (without the
  advanced lighting), closer to how the game looks on PC but slower on the
  heaviest tracks. Turn it on by setting `vita_shader_renderer="true"` in the
  `<Video` section of `ux0:data/supertuxkart/config-0.10/config.xml`; it is
  read when the game starts.

## Installing

1. Install the VPK (title ID `STKV00001`).
2. Copy the game data to `ux0:data/supertuxkart/stk-data/`: the `assets/data`
   folder of the official Android package (`SuperTuxKart-1.5.apk` from the
   [1.5 release](https://github.com/supertuxkart/stk-code/releases/tag/1.5)),
   then the `data` folder of this repository over it, replacing the files that
   are in both.

The game keeps its config and saves in `ux0:data/supertuxkart/`.

## Building

You need the [VitaSDK](https://vitasdk.org) (the `vitasdk/vitasdk` Docker
image works) with the `openssl` package instead of `openssl-1.1.1`, and vitaGL
from the `stk-port` branch of
[edgarlugo/vitaGL](https://github.com/edgarlugo/vitaGL/tree/stk-port), which
has the fixes this port needs and the GLSL ES 3.00 support of the shader-based
renderer:

```sh
vdpm pacman -- -Rdd --noconfirm openssl-1.1.1
vdpm pacman -- -S --noconfirm vita/openssl
git clone -b stk-port https://github.com/edgarlugo/vitaGL
make -C vitaGL HAVE_SHADER_CACHE=1 HAVE_GLSL_UBOS=1 HAVE_GLSL_TEXTURE_SIZE=1 install
```

Then build the game:

```sh
cmake -S . -B build -DUSE_VITA=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

`build/supertuxkart.vpk` is the VPK and `build/eboot.bin` the executable alone.

The rest of this file is the README of SuperTuxKart.

---

# SuperTuxKart
[![Linux build status](https://github.com/supertuxkart/stk-code/actions/workflows/linux.yml/badge.svg)](https://github.com/supertuxkart/stk-code/actions/workflows/linux.yml)
[![Apple build status](https://github.com/supertuxkart/stk-code/actions/workflows/apple.yml/badge.svg)](https://github.com/supertuxkart/stk-code/actions/workflows/apple.yml)
[![Windows build status](https://github.com/supertuxkart/stk-code/actions/workflows/windows.yml/badge.svg)](https://github.com/supertuxkart/stk-code/actions/workflows/windows.yml)
[![Switch build status](https://github.com/supertuxkart/stk-code/actions/workflows/switch.yml/badge.svg)](https://github.com/supertuxkart/stk-code/actions/workflows/switch.yml)
[![#supertuxkart on the libera IRC network](https://img.shields.io/badge/libera-%23supertuxkart-brightgreen.svg)](https://web.libera.chat/?channels=#supertuxkart)

SuperTuxKart is a free kart racing game. It focuses on fun and not on realistic kart physics. Instructions can be found on the in-game help page.

The SuperTuxKart homepage can be found at <https://supertuxkart.net/>. There is also our [FAQ](https://supertuxkart.net/FAQ) and information on how get in touch with the [community](https://supertuxkart.net/Community).

The latest release binaries can be found [here](https://github.com/supertuxkart/stk-code/releases/latest), and those of preview releases [here](https://github.com/supertuxkart/stk-code/releases/preview).

## Hardware Requirements
Any hardware that supports OpenGL >= 3.3 or OpenGL ES >= 3.0 should be able to run SuperTuxKart. For Android, Android 5.0 or greater is required.

This includes dedicated GPUs from Nvidia or AMD released after 2010, integrated GPUs released after 2012 and Android phones released after 2014.

The game will run on some very weak devices, but a better GPU allows to run smoothly using higher quality graphics and a higher resolution, and a better CPU helps to keep a smooth playing experience in online multiplayer.

Please check the "What are the hardware requirements?" question in our [FAQ](https://supertuxkart.net/FAQ) for more information.

## License
The software is released under the GNU General Public License (GPL) which can be found in the file [`COPYING`](/COPYING) in the same directory as this file.

---

## Building from source

Building instructions can be found in [`INSTALL.md`](/INSTALL.md)

## Contributing code

**To contribute code to the official STK repository, please review the ['How to contribute code'](https://supertuxkart.net/How_to_contribute_code) guide.** It contains important guidelines to make the process smoother and maximize the chances that your contribution is accepted.

## 3D coordinates
A reminder for those who are looking at the code and 3D models:

SuperTuxKart: X right, Y up, Z forwards

Blender: X right, Y forwards, Z up

The export utilities perform the needed transformation, so in Blender you just work with the XY plane as ground, and things will appear fine in STK (using XZ as ground in the code, obviously).
