<p align="center">
  <img src="./docs/banner.png" alt="Switch-NewPipe" width="848"/>
</p>

<p align="center">
  <strong>A free, open-source YouTube client for Nintendo Switch homebrew.</strong><br>
  Inspired by <a href="https://github.com/TeamNewPipe/NewPipe">NewPipe</a> &mdash; no Google account required, no ads, no tracking.
</p>

<p align="center">
  <a href="./README_kr.md">한국어</a>
</p>

---

## Screenshots

|           Home Feed          |          Player (720p)         |
| :--------------------------: | :----------------------------: |
| ![home](./docs/preview2.jpg) | ![player](./docs/preview1.jpg) |

## Install

1. Make sure your Switch has **Atmosphere CFW** with the Homebrew Menu
2. Download `switch_newpipe.nro` from the [latest release](../../releases/latest)
3. Copy it to `sdmc:/switch/switch_newpipe.nro`
4. Launch from the Homebrew Menu

## Fork Changes

This fork adds a number of playback, UI, and usability improvements to the original project.

### Progressive Playback

Playback now starts after approximately **1 MB** has been downloaded instead of waiting for the entire stream to become available.

The remaining data continues downloading in the background while you watch.

### Tokenless 720p Streaming

Adds **tokenless 720p streaming** through the Android VR UMP path using **4 MB chunks**.

Ratebypass fallbacks also support chunked streaming, improving compatibility with streams that cannot use the primary UMP path.

### Seek Outside the Buffer

Seeking is no longer restricted to the portion of the video that has already been downloaded.

* **Left / Right:** Seek ±10 seconds
* **LB / RB:** Seek ±60 seconds
* Seeking outside the downloaded range automatically restarts the downloader from the target byte position.

This allows jumping forward through a video without waiting for the entire section between the current position and the target to download.

### In-Playback Quality Picker

Press **X** during playback to open the quality selection menu.

Available options include:

* **AUTO**
* Any available video resolution

Changing quality preserves the current playback position.

### Continue Watching

Playback position is automatically saved:

* Every **10 seconds**
* When playback is paused
* When exiting the player

Videos resume from the last saved position when opened again.

### Enlarged Two-Column Selection UI

The selection interface has been redesigned for easier navigation on the Switch.

* Larger selectable items
* Two-column layout
* Better use of available screen space
* Easier navigation with a controller

### Full-Screen Tab Menu

Press **ZR** to open a full-screen menu with all five tabs (Home, Search, Subscriptions, Library, Settings). The sidebar stays hidden while browsing content, so the menu is the only way to switch tabs.

### Infinite Home Feed

The Home feed loads more recommendation pages as you scroll past the last rows, making the feed effectively infinite. Continuation responses are parsed regardless of their container structure, including the modern WEB-client format.

---

## What You Can Do

* Browse **Home**, **Search**, **Subscriptions**, **Library**, and **Settings**
* Watch YouTube at **720p**
* Start playback while the stream is still downloading
* Seek through videos, including positions outside the downloaded buffer
* Change playback quality without leaving the player
* Search for any video and play it immediately
* Log in with cookies to see your subscriptions and personalized recommendations
* Save watch history and favorites locally
* Resume videos from their previous playback position
* Scroll the Home feed infinitely — more recommendations load as you reach the bottom
* English & Korean UI

## Controls

### Main UI

| Button | Action                                   |
| ------ | ---------------------------------------- |
| `A`    | Play video from list                     |
| `Y`    | Open video details                       |
| `X`    | Refresh / Reset defaults                 |
| `RB`   | Manage login session (Subscriptions tab) |
| `ZR`   | Open the full-screen tab menu            |

### Player

| Button         | Action                 |
| -------------- | ---------------------- |
| `A`            | Pause / Resume         |
| `B`            | Exit player            |
| `Up / Down`    | Volume                 |
| `Left / Right` | Seek 10 seconds        |
| `LB / RB`      | Seek 60 seconds        |
| `X`            | Open quality selection |
| `Y`            | Toggle OSD overlay     |

Seeking outside the currently downloaded range automatically restarts the downloader at the requested position.

## Login (Optional)

Switch-NewPipe uses cookie import for YouTube login. No OAuth or Google sign-in required.

**How to set up:**

1. Export your YouTube cookies from a browser using a cookie export extension
2. Save the file as `sdmc:/switch/switch_newpipe_auth.txt`
3. Restart the app

Supported formats: raw `Cookie` header, JSON `{"cookie_header":"..."}`, or Netscape `cookies.txt`.

Once logged in, your **Subscriptions** tab and **personalized Home recommendations** will be available.

## Playback Quality

Configure the default playback mode in **Settings**:

| Mode              | Description                                                        |
| ----------------- | ------------------------------------------------------------------ |
| **Standard 720p** | Best quality. Tries 720p streaming first and falls back gracefully |
| **Compatibility** | Prefers progressive MP4 (video + audio combined)                   |
| **Data Saver**    | Lower quality around 480p to save bandwidth                        |

The player also provides an **in-playback quality picker** through `X`, allowing the quality to be changed without leaving the video.

## Data Files

All data is stored on your SD card:

| File                                        | Purpose                          |
| ------------------------------------------- | -------------------------------- |
| `sdmc:/switch/switch_newpipe.log`           | Debug log                        |
| `sdmc:/switch/switch_newpipe_settings.json` | Settings                         |
| `sdmc:/switch/switch_newpipe_library.json`  | Watch history & favorites        |
| `sdmc:/switch/switch_newpipe_session.json`  | Login session                    |
| `sdmc:/switch/switch_newpipe_auth.txt`      | Cookie import (you provide this) |

## Build from Source

Requires Docker and a host C++ compiler.

```bash
# Full build (portlibs + app)
./build.sh

# App only (after first full build)
./build.sh --app-only

# Clean everything
./build.sh --clean
```

Output: `cmake-build-switch/switch_newpipe.nro`

<details>
<summary>Host validation tools (for development)</summary>

```bash
make host
./build/host/switch_newpipe_host
./build/host/switch_newpipe_host --search Zelda
./build/host/switch_newpipe_host --resolve 'https://www.youtube.com/watch?v=dQw4w9WgXcQ'
```

</details>

## Known Limitations

* No in-app Google OAuth (cookie import only)
* Channel pages are not fully browsable yet
* Comments and playlists load first page only

## Tech Stack

* **UI**: [Borealis](https://github.com/natinusala/borealis) (native Switch UI framework)
* **Playback**: mpv + FFmpeg (hardware-accelerated on Switch)
* **Networking**: libcurl + custom YouTube Innertube API client
* **Build**: CMake, Docker, devkitPro toolchain

## License

This project is for educational purposes. It is not affiliated with YouTube, Google, or NewPipe.
