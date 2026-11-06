# Browser port handoff

The browser port lives on `codex/destructo-web` in the `silentpickle/drdestructo2` fork. Start with the [build and play instructions](../README.md#browser-edition). Emscripten, Allegro, and Lua versions are pinned in [the web build](../web/CMakeLists.txt). The build expects Emscripten's SDL2, PNG, and FreeType ports to be installed before CMake runs. Use fresh build directories after changing SDKs.

## Where to make changes

| Work | Start here |
| --- | --- |
| Browser launch, controls, audio, and saves | [web/index.html](../web/index.html), [web/game.js](../web/game.js), and [AllegroInput5.cpp](../src/Engine/AllegroInput5.cpp) |
| Frame timing, state transitions, and rendering | [Game.cpp](../src/Game.cpp) and [AllegroRender5.cpp](../src/Engine/AllegroRender5.cpp) |
| Campaign, enemies, and collision behavior | [data/Levels](../data/Levels), [ai.lua](../data/Protos/ai.lua), and [LuaAI.cpp](../web/LuaAI.cpp) |
| Static files and attribution | [stage_assets.py](../web/stage_assets.py), [original credits](ReadMe.txt), and [font license](../web/BalsamiqSans-OFL.txt) |

The browser's virtual filesystem uses case-sensitive `Data/…` paths. Match XML asset names to the files under `data/` exactly. Clear the WebGL backbuffer at the start of a frame so the browser can present the drawing after the callback returns. Pointer controls send events through the same game input handlers as the physical keyboard.

## Verify before releasing

1. Run a Release build over HTTP. Check that the title, briefing, and Banyo render, and that audio starts after **Play**. Verify keyboard and mouse input with the on-screen controls both shown and hidden.
2. Use a Debug build to see game state in the browser console. Enter `game.qa = {}` after Play, then inspect `game.qa.frame`. Compare its state ID with [EGameState](../src/GameContext.h), and check `damage`, `hits`, and `targetHits` while playing. For Banyo, steer, shoot down an aircraft, see the wreck damage the target, and advance from the results to the next briefing with Fire.
3. Use F2 in Debug to visit all 21 levels and reach the ending. Give later enemies time to run their scripts. Check `/saves/game.log` in the WebAssembly filesystem for load and Lua errors. Record any skipped levels as unverified.
4. Reload the same URL after changing a binding and unlocking a level. Confirm that progress, settings, and a submitted score persist. On iPhone Safari, test touch menus, two held controls, rotation, and background/resume. On desktop, enable **Auto-fire** and steer with the mouse. Release and cancel pointers and check that the plane stops receiving those controls.

The first Chrome Debug playthrough used regular flight controls and reached Banyo's damage threshold with 80 hits, 64 target impacts, and 800 points. The run reached the results state. The results screen previously reported the same state ID as the campaign ending, which stopped the test driver before the next briefing. The two states now report distinct IDs. Chrome also reached Banyo with mouse-only input and in phone emulation. Both runs verified auto-fire, steering, input release, and pause/resume. Phone emulation also verified menu taps, two simultaneous controls, and the landscape layout. See the remaining checks below before claiming campaign completion or physical iPhone compatibility.

## Publish to GitHub Pages

The project URL is `https://silentpickle.github.io/drdestructo2/`. Publish the **Release** output to the root of `gh-pages`, with `.nojekyll` in that branch. Keep the source branch and generated branch separate. Build from the intended source revision, then use a separate checkout of `gh-pages` to replace its generated content with `dist/`. Commit the build with the source commit SHA in the message and push normally. Configure **Settings → Pages → Deploy from a branch → gh-pages → /(root)** on the first deployment.

Wait for the Pages run to succeed. Check the project URL and confirm that the WebAssembly file and asset files load from the `/drdestructo2/` subpath. Start the deployed game and reach Banyo before handing off the link. [GitHub Pages publishing sources](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)

## Outstanding checks

- Run through every campaign level and the ending in one Debug smoke test.
- Verify score submission and binding changes survive reload.
- Play on a physical iPhone and verify Safari audio after backgrounding.
