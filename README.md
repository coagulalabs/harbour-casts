# Casts

Native podcast player for [Sailfish OS](https://sailfishos.org/). No trackers, no ads, full offline support.

**Version:** 1.1.1 · **License:** [BSD-3-Clause](LICENSE) · **Target:** Jolla C2 / Sailfish OS 5.x (aarch64)

## Features

- Subscribe via RSS URL or OPML import
- Stream or **download** episodes for offline listening
- **Show notes** per episode (plain-text, stored locally)
- **Cached cover art** for fast subscription list scrolling
- Playback queue with auto-advance
- Variable speed (0.75×–2×) and sleep timer with countdown
- Resume playback where you left off
- Pull to refresh feeds; load more for large catalogs

## Install

Grab the latest RPM from [Releases](https://github.com/coagulalabs/harbour-casts/releases) (or OpenRepos when published) and install on device:

```bash
pkcon install-local harbour-casts-1.1.1-1.aarch64.rpm
```

## Build

Requires the [Sailfish SDK](https://sailfishos.org/wiki/Application_SDK) with target `SailfishOS-5.0.0.62-aarch64` (or compatible) and Docker access for `sfdk`.

```bash
./scripts/build.sh          # build with sfdk
./scripts/build.sh --clean  # clean rebuild
./scripts/package.sh        # build + copy RPM to dist/
```

Override the SDK binary if needed:

```bash
SFDK=/path/to/sfdk ./scripts/build.sh
```

## Data locations

| Path | Purpose |
|------|---------|
| `~/.local/share/harbour-casts/` | SQLite database |
| `~/.cache/harbour-casts/artwork/` | Cached podcast artwork |
| `~/Downloads/Casts/` | Downloaded episode audio |

## Stack

- C++ / Qt 5 — RSS parser, SQLite, `QNetworkAccessManager`, `QMediaPlayer`
- Silica QML UI
- Sailjail profile for Internet, audio, downloads, and user dirs

## License

[BSD 3-Clause](LICENSE)
