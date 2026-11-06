"use strict";

const cabinet = document.querySelector("main");
const canvas = document.querySelector("#canvas");
const fullscreen = document.querySelector("#fullscreen");
const overlay = document.querySelector("#overlay");
const status = document.querySelector("#status");
const play = document.querySelector("#play");
const warning = document.querySelector("#warning");
const controls = document.querySelector("#controls");
const controlsToggle = document.querySelector("#controls-toggle");
const autoFireButton = document.querySelector("#auto-fire");
const fireAction = 3;
const pointers = new Map();
const activeActions = new Set();
let autoFire = false;
let canvasTouch;
const warnings = new Map();
let game;
let audio;
let mounted = false;
let dirty = false;
let syncing = false;
let started = false;
let paused = false;

function syncAction(action) {
    const held = [...pointers.values()].some(value => value === action);
    const down = held || (action === fireAction && autoFire);
    if (activeActions.has(action) === down) return;
    if (down) activeActions.add(action);
    else activeActions.delete(action);
    controls.querySelector(`[data-action="${action}"]`)?.toggleAttribute("data-held", down);
    if (started && !paused) game._dd2_web_action(action, Number(down));
}

function clearControls() {
    pointers.clear();
    canvasTouch = undefined;
    autoFire = false;
    autoFireButton.setAttribute("aria-pressed", "false");
    for (const action of activeActions) syncAction(action);
}

function showControls(show) {
    if (!show) clearControls();
    controls.hidden = !show;
    controlsToggle.textContent = show ? "Hide controls" : "Show controls";
    controlsToggle.setAttribute("aria-expanded", String(show));
}

function tapAction(action) {
    if (!started || paused || activeActions.has(action)) return;
    game._dd2_web_action(action, 1);
    game._dd2_web_action(action, 0);
}

controlsToggle.addEventListener("click", () => showControls(controls.hidden));
fullscreen.hidden = !document.fullscreenEnabled;
fullscreen.addEventListener("click", async () => {
    try {
        if (document.fullscreenElement === cabinet) await document.exitFullscreen();
        else await cabinet.requestFullscreen();
    } catch (error) {
        console.warn("Destructo full screen:", error);
        warn("fullscreen", "Full screen is unavailable in this browser window.");
    }
});
document.addEventListener("fullscreenchange", () => {
    const expanded = document.fullscreenElement === cabinet;
    fullscreen.textContent = expanded ? "Exit full screen" : "Full screen";
    fullscreen.setAttribute("aria-pressed", String(expanded));
});
autoFireButton.addEventListener("click", () => {
    if (!started || paused) return;
    autoFire = !autoFire;
    autoFireButton.setAttribute("aria-pressed", String(autoFire));
    syncAction(fireAction);
});

for (const button of controls.querySelectorAll("button[data-action]")) {
    const action = Number(button.dataset.action);
    button.addEventListener("pointerdown", event => {
        if (!started || paused || event.button !== 0) return;
        event.preventDefault();
        button.setPointerCapture(event.pointerId);
        pointers.set(event.pointerId, action);
        syncAction(action);
    });
    function release(event) {
        if (pointers.delete(event.pointerId)) syncAction(action);
    }
    button.addEventListener("pointerup", release);
    button.addEventListener("pointercancel", release);
    button.addEventListener("lostpointercapture", release);
    button.addEventListener("click", event => {
        if (event.detail === 0) tapAction(action);
    });
}
for (const button of controls.querySelectorAll("button[data-tap]")) {
    button.addEventListener("click", () => tapAction(Number(button.dataset.tap)));
}

function warn(kind, message) {
    warnings.set(kind, message);
    warning.textContent = [...warnings.values()].join(" ");
    warning.hidden = false;
}

function storageUnavailable(error) {
    mounted = false;
    console.warn("Destructo save storage:", error);
    warn("storage", "Browser storage is unavailable. Progress and settings may be lost when you close the page.");
}

function saveProgress() {
    if (!mounted) return;
    dirty = true;
    if (syncing) return;

    const flush = () => {
        dirty = false;
        syncing = true;
        game.FS.syncfs(false, (error) => {
            syncing = false;
            if (error) storageUnavailable(error);
            else if (dirty) flush();
        });
    };
    flush();
}

async function mountSaves() {
    const fs = game.FS;
    fs.mkdir("/saves");
    let attached = false;
    try {
        fs.mount(game.IDBFS, {}, "/saves");
        attached = true;
        await new Promise((resolve, reject) => fs.syncfs(true, (error) => error ? reject(error) : resolve()));
        mounted = true;
    } catch (error) {
        if (attached) fs.unmount("/saves");
        storageUnavailable(error);
    }
}

function createAudio(context, buffers) {
    const master = context?.createGain();
    const buses = context ? [context.createGain(), context.createGain()] : [];
    const samples = new Map();
    let nextId = 1;
    if (master) {
        master.connect(context.destination);
        for (const bus of buses) bus.connect(master);
    }

    function stop(id) {
        const sample = samples.get(id);
        if (!sample?.source) return false;
        const source = sample.source;
        sample.source = null;
        source.stop();
        source.disconnect();
        return true;
    }

    return {
        create(path, type) {
            const buffer = buffers.get(path);
            if (!buffer || !context) return 0;
            const id = nextId++;
            const gain = context.createGain();
            const stereo = context.createStereoPanner();
            gain.connect(stereo);
            stereo.connect(buses[type]);
            samples.set(id, { buffer, gain, stereo, rate: 1, loop: false, source: null });
            return id;
        },
        remove(id) {
            const sample = samples.get(id);
            if (!sample) return;
            stop(id);
            sample.gain.disconnect();
            sample.stereo.disconnect();
            samples.delete(id);
        },
        play(id) {
            const sample = samples.get(id);
            if (!sample) return false;
            stop(id);
            const source = context.createBufferSource();
            source.buffer = sample.buffer;
            source.loop = sample.loop;
            source.playbackRate.value = sample.rate;
            source.connect(sample.gain);
            source.onended = () => {
                if (sample.source === source) sample.source = null;
                source.disconnect();
            };
            sample.source = source;
            source.start();
            return true;
        },
        stop,
        playing(id) { return Boolean(samples.get(id)?.source); },
        set(id, key, value) {
            const sample = samples.get(id);
            if (!sample) return;
            switch (key) {
                case "volume": sample.gain.gain.value = value; break;
                case "pan": sample.stereo.pan.value = value; break;
                case "rate":
                    sample.rate = value;
                    if (sample.source) sample.source.playbackRate.value = value;
                    break;
                case "loop":
                    sample.loop = value;
                    if (sample.source) sample.source.loop = value;
                    break;
            }
        },
        volume(type, value) {
            const bus = type < 0 ? master : buses[type];
            if (bus) bus.gain.value = value;
        }
    };
}

async function loadAudio() {
    const buffers = new Map();
    try {
        audio = new AudioContext();
        // Request audio during the click. Asset decoding can proceed if playback is suspended.
        audio.resume().catch(console.warn);
        const response = await fetch("audio.json");
        if (!response.ok) throw Error("Audio manifest could not be loaded");
        const manifest = await response.json();
        const results = await Promise.allSettled(Object.entries(manifest).map(async ([path, url]) => {
            const file = await fetch(url);
            if (!file.ok) throw Error(`Audio could not be loaded: ${path}`);
            buffers.set(path, await audio.decodeAudioData(await file.arrayBuffer()));
        }));
        if (results.some(result => result.status === "rejected")) {
            console.warn("Destructo audio:", results.filter(result => result.status === "rejected"));
            warn("audio", "Some game audio could not be loaded.");
        }
    } catch (error) {
        console.warn("Destructo audio:", error);
        warn("audio", "Sound is unavailable. You can still play.");
    }
    game.audio = createAudio(audio, buffers);
}

function stopWithMessage(message) {
    clearControls();
    showControls(false);
    controlsToggle.hidden = true;
    status.textContent = message;
    overlay.hidden = false;
    play.textContent = "Reload";
    play.disabled = false;
    play.onclick = () => location.reload();
    started = false;
}

function pauseGame() {
    if (!started || paused) return;
    clearControls();
    paused = true;
    game._dd2_set_paused(1);
    if (audio?.state === "running") audio.suspend().catch(console.warn);
    status.textContent = "Paused";
    play.textContent = "Resume";
    overlay.hidden = false;
}

async function start() {
    try {
        play.disabled = true;
        status.textContent = "Preparing sound…";
        await loadAudio();
        const result = game.callMain([]);
        if (result !== undefined && result !== 0) throw Error(`Game startup exited with status ${result}`);
        started = true;
        status.textContent = "";
        overlay.hidden = true;
        controlsToggle.hidden = false;
        showControls(matchMedia("(any-pointer: coarse)").matches);
        canvas.focus({preventScroll: true});
        if (document.hidden) pauseGame();
    } catch (error) {
        console.error(error);
        stopWithMessage("The game could not start. Reload to try again.");
    } finally {
        play.disabled = false;
    }
}

play.onclick = () => {
    if (!started) {
        void start();
    } else if (paused) {
        if (audio?.state === "suspended") audio.resume().catch(console.warn);
        paused = false;
        game._dd2_set_paused(0);
        overlay.hidden = true;
        canvas.focus({preventScroll: true});
    }
};

document.addEventListener("keydown", event => {
    if (event.key === "Enter" && !started && !play.disabled &&
        !event.target.closest("button, a, input, textarea, select"))
    {
        event.preventDefault();
        play.click();
    }
});
window.addEventListener("blur", pauseGame);
canvas.addEventListener("blur", event => {
    if (event.relatedTarget && !controls.contains(event.relatedTarget) &&
        event.relatedTarget !== controlsToggle && event.relatedTarget !== fullscreen)
        pauseGame();
});
document.addEventListener("visibilitychange", () => {
    if (document.hidden) pauseGame();
});
canvas.addEventListener("pointerdown", event => {
    if (event.pointerType !== "touch" || !event.isPrimary || !started || paused) return;
    event.preventDefault();
    canvasTouch = {id: event.pointerId, x: event.clientX, y: event.clientY};
});
canvas.addEventListener("pointercancel", event => {
    if (canvasTouch?.id === event.pointerId) canvasTouch = undefined;
});
canvas.addEventListener("pointerup", event => {
    if (!canvasTouch || canvasTouch.id !== event.pointerId) return;
    const tap = canvasTouch;
    canvasTouch = undefined;
    if (!started || paused || Math.hypot(event.clientX - tap.x, event.clientY - tap.y) > 18) return;
    const bounds = canvas.getBoundingClientRect();
    game._dd2_web_tap((event.clientX - bounds.left) / bounds.width,
        (event.clientY - bounds.top) / bounds.height);
});
canvas.addEventListener("keydown", (event) => {
    if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", " "].includes(event.key)) event.preventDefault();
});
canvas.addEventListener("webglcontextlost", (event) => {
    event.preventDefault();
    pauseGame();
    stopWithMessage("The browser lost the graphics context. Reload to resume from your last save.");
});

(async () => {
    try {
        game = await createDestructo({
            canvas,
            saveProgress,
            onGameExit: () => stopWithMessage("Game closed."),
            onAbort: () => stopWithMessage("The game encountered an error. Reload to try again.")
        });
        await mountSaves();
        status.textContent = "Take to the skies.";
        play.disabled = false;
    } catch (error) {
        console.error(error);
        stopWithMessage("The game files could not be loaded. Reload to try again.");
    }
})();
