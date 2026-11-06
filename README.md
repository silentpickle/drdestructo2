# Return of Dr. Destructo

Return of Dr. Destructo is a cross-platform, open-source remake of ZX Spectrum game "Island of Dr. Destructo" (1987).

Binary downloads for the game could be found at: http://zxstudio.org/projects/drdestructo/

Source code for this game is licensed under MIT license and art is CC-BY-SA. Please consult docs/LICENSE-CODE.txt and docs/LICENSE-ART.txt for details.

## Browser edition

The web build uses the original C++ game, its 21 campaign levels, sprites, and Lua enemy scripts. Play with a keyboard, mouse, or the on-screen controls. Progress, high scores, and options are stored in the browser for each play URL.

Install CMake 3.24 or later, Ninja, Python 3.8 or later, FFmpeg with the LAME MP3 encoder, and OpenMPT's `openmpt123`. On macOS, these are available with `brew install cmake ninja python ffmpeg libopenmpt`. Install Emscripten 4.0.23 using the [official SDK](https://github.com/emscripten-core/emsdk).

```sh
# In your Emscripten SDK checkout
./emsdk install 4.0.23
./emsdk activate 4.0.23
source ./emsdk_env.sh
embuilder build sdl2 libpng freetype

# Back in the game checkout
emcmake cmake -S web -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python3 -m http.server 8765 --bind 127.0.0.1 --directory dist
```

Open [the local game](http://127.0.0.1:8765/) and click **Play**. Press Enter on the title screen. In flight, use the left and right arrows to turn, up to boost, Q to fire, A to drop a bomb, and Esc to pause. The game pauses when it loses focus. On a touch screen, use the on-screen buttons. With a mouse, click **Show controls** and turn on **Auto-fire** to shoot while steering.

The `dist/` directory is the complete static release. Serve it over HTTP(S) with `.wasm` mapped to `application/wasm`. The first build downloads checksum-pinned Allegro and Lua sources. The build renders the tracker files to MP3 and sound effects to WAV for Web Audio. The original tracker files stay in the repository.

For campaign checks, configure a separate build with `-DCMAKE_BUILD_TYPE=Debug -Ddist="$PWD/build-web-debug/dist"`. This build includes the existing F2 command to finish a level for testing. Release builds omit that command.

For verification and deployment, see the [browser port handoff](docs/WEB_HANDOFF.md).

## Legacy native build

Building Return of Dr. Destructo is not a very easy process, because it was mostly custom-tailored for my own needs and tastes, and also to finally release the damn thing :)

I use CMake for build system, and the only non-standard dependency which is not provided along with game's sources is Allegro game library (http://allegro.cc).

Unfortunately, you will need a custom patch for Allegro library, which is provided in the root folder. Also, the game expects Allegro to be built in a particular manner. Please enable WANT_MONOLITH for all platforms. For Linux and MacOS X builds, a statically linked build is expected.

After building Allegro with the patch, you should place the custom binaries and headers into project's root directory:

### Windows:

allegro_win32\bin - DLLs

allegro_win32\include - header files

allegro_win32\lib - .lib files

### Linux & MacOS X:

allegro_unix\include - header files

allegro_unix\lib - .a files

## A note on building for MacOS X with XCode:

I only tested Makefile-based build for MacOS X. Though the game must build with XCode too, I do not guarantee it will run without further tweaking of project.