"""Prepare case-sensitive game data and browser-decodable audio."""
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def convert(source, destination, ffmpeg, openmpt):
    input_file = source
    with tempfile.TemporaryDirectory() as tmp:
        if source.suffix == ".it":
            input_file = Path(tmp) / "track.wav"
            subprocess.run(
                [openmpt, "--batch", "--quiet", "--repeat", "0",
                 "--samplerate", "48000", "--output", input_file, source],
                check=True,
            )
        codec = ["-c:a", "libmp3lame", "-q:a", "3"] if destination.suffix == ".mp3" else [
            "-c:a", "pcm_s16le"
        ]
        subprocess.run(
            [ffmpeg, "-nostdin", "-loglevel", "error", "-y", "-i", input_file, *codec, destination],
            check=True,
        )


def main(root, stage, dist, ffmpeg, openmpt, allegro, lua):
    web = root / "web"
    data = root / "data"
    stage.mkdir(parents=True, exist_ok=True)
    dist.mkdir(parents=True, exist_ok=True)
    (stage / "prepared").unlink(missing_ok=True)
    if (stage / "Data").exists():
        shutil.rmtree(stage / "Data")
    shutil.copytree(data, stage / "Data", ignore=shutil.ignore_patterns("*.it", "*.ogg", "*.wav", "*.otf"))
    shutil.copy2(web / "BalsamiqSans-Regular.ttf", stage / "Data/Fonts")

    sounds = dist / "audio"
    sounds.mkdir(exist_ok=True)
    manifest = {}
    for directory, pattern, extension in [("Music", "*.it", ".mp3"), ("Sounds", "*", ".wav")]:
        for source in sorted((data / directory).glob(pattern)):
            if source.suffix not in {".it", ".ogg", ".wav"}:
                continue
            destination = sounds / (source.stem + extension)
            convert(source, destination, ffmpeg, openmpt)
            manifest["Data/" + source.relative_to(data).as_posix()] = destination.relative_to(dist).as_posix()
    (dist / "audio.json").write_text(json.dumps(manifest, indent=2) + "\n")

    shutil.copy2(data / "dd2_icon.ico", dist / "favicon.ico")
    for name in ["index.html", "game.js"]:
        shutil.copy2(web / name, dist / name)
    licenses = dist / "licenses"
    licenses.mkdir(exist_ok=True)
    for source, name in [
        (root / "docs/ReadMe.txt", "CREDITS.txt"),
        (root / "docs/LICENSE-ART.txt", "LICENSE-ART.txt"),
        (root / "docs/LICENSE-CODE.txt", "LICENSE-CODE.txt"),
        (web / "BalsamiqSans-OFL.txt", "BalsamiqSans-OFL.txt"),
        (allegro / "LICENSE.txt", "Allegro.txt"),
        (lua / "doc/readme.html", "Lua.html"),
        (root / "src/tinyxml/tinyxml.h", "TinyXML.h"),
    ]:
        shutil.copy2(source, licenses / name)
    (stage / "prepared").touch()


if __name__ == "__main__":
    root, stage, dist, ffmpeg, openmpt, allegro, lua = sys.argv[1:]
    main(Path(root), Path(stage), Path(dist), ffmpeg, openmpt, Path(allegro), Path(lua))
