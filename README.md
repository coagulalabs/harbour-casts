# Casts — harbour-casts

Native podcast player for Sailfish OS. No trackers, no ads, full offline support.

## Features

- Subscribe via RSS URL or OPML import
- Stream or **download** episodes for offline listening
- **Show notes** per episode (plain-text, stored locally)
- **Cached cover art** for fast subscription list scrolling
- Playback queue with auto-advance
- Variable speed (0.75×–2×) and sleep timer with countdown
- Resume playback where you left off
- Pull to refresh feeds; load more for large catalogs

## Build & install

From the SailfishOS repo root:

```bash
./scripts/deploy-casts.sh          # build + install on phone
./scripts/deploy-casts.sh --clean  # clean rebuild
./scripts/package-casts.sh         # RPM → dist-casts/
```

Requires Sailfish SDK target `SailfishOS-5.0.0.62-aarch64` (or compatible).

Standalone source repo: [github.com/crows/harbour-casts](https://github.com/crows/harbour-casts)

## Publish (GitHub + OpenRepos)

```bash
./scripts/publish-casts.sh                           # build + cache RPM, sync to harbour-casts repo
./scripts/publish-casts.sh --github                  # + GitHub release (GITHUB_TOKEN or gh)
OPENREPOS_USERNAME=... OPENREPOS_PASSWORD=... \
  ./scripts/publish-casts.sh --openrepos             # + OpenRepos upload
```

Cached RPM: `dist-casts/harbour-casts-1.1.0-1.aarch64.rpm`

OpenRepos account needs the **publisher** role at [openrepos.net/user/register](https://openrepos.net/user/register).

## Data locations

| Path | Purpose |
|------|---------|
| `~/.local/share/harbour-casts/` | SQLite database |
| `~/.cache/harbour-casts/artwork/` | Cached podcast artwork |
| `~/Downloads/Casts/` | Downloaded episode audio |

## Stack

- C++ / Qt 5: RSS parser, SQLite, QNetworkAccessManager, QMediaPlayer
- Silica QML UI
- Target: Jolla C2 / Sailfish OS 5.x (aarch64)

## License

BSD-3-Clause
