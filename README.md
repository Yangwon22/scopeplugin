# ScopePlugin

Unreal Engine 5.7 editor plugin that records every virtual-production shot: camera,
tracking, ICVFX and nDisplay settings, lights, post-process state and scene actors,
serialized to JSON and posted to a shot database. The companion web app
(Scope Web) stores the takes with NDC and inner-frustum thumbnails.

## What it collects

- CineCamera: transform, FOV, aperture, focus distance, focal length, sensor size
- Live Link: connection state, subject names, tracked transform
- Post Process Volume, Sky / Directional lights
- nDisplay: config name, resolution, genlock, FPS (reflection based, no hard dependency)
- ICVFX: inner-frustum ratio, overscan, soft edge, light cards, chromakey
- Optional data filters and an Actor Picker for per-shot actor transforms
- Screenshots of the inner frustum and NDC view, uploaded with the take

## Panel

Session ID, Shot / Scene / Take, Status (OK / NG / CIRCLE) and a Save button.
Filters are stored in `GConfig` INI so they survive editor restarts.

## Requirements

- Unreal Engine 5.7, Windows, Visual Studio 2022
- Optional plugins: CinematicCamera, LiveLink, nDisplay, LevelSequenceEditor
- A Scope Web endpoint (default `http://localhost:3000/api/shot-save`); see `spec.md`

## Author

Won Yang — https://wonvp.com
