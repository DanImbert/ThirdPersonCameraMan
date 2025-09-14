ThirdPersonCameraMan (Code‑only)

UE 5.6 multiplayer sample implementing a simple “film director” flow:
- Operator picks up camera rigs placed in the world (one at a time)
- Camera “turns on” (SceneCapture2D → transient RenderTarget)
- Viewer automatically sees what the active camera films
- Switching: bump the active rig into another rig to switch
- Drop: Operator presses `Q` to drop the current rig (feed stops; viewer returns to pawn)

Requirements
- Unreal Engine 5.6
- This repository is code‑only: no Content/ assets are tracked. Rigs auto‑create transient render targets at runtime.

Open & Run
- Open `ThirdPersonCameraMan.uproject`
- Play In Editor with 2 players (Listen Server)
  - Window → Play → Advanced Settings
  - Number of Players: 2
  - Net Mode: Listen Server
- In the level, place a few `BP_CameraRig` instances
  - Give each a unique label (e.g., BP_CameraRig1/2/3) or set `RigLabel` in Details

Controls (Operator)
- Overlap a rig to pick it up (attaches to head socket if present)
- Walk the active rig into another rig to switch
- `Q` — Drop current rig (capture OFF, detach, viewer reverts)

What You Should See
- Pickup toast: `player N picked up :  <RigLabel/ActorLabel>`
- Switch toast (clients): `Switched to :  <RigLabel/ActorLabel>`
- Drop toast (clients): `Active camera: none`
- Viewer blends to the active rig; when dropped, viewer returns to their pawn

Implementation Notes
- Server‑authoritative pickup/switch/drop (GameMode + GameState)
- One camera at a time
  - Old rig is detached and capture disabled before enabling the new one
  - Global switch lock (0.15 s) prevents ping‑pong on overlaps
- Switching reliability
  - Switch sphere overlaps both WorldDynamic and WorldStatic; radius = 150 by default
- No RT asset required
  - Each rig creates a transient RT on BeginPlay (or clones size from an assigned RT asset)

Code Map
- `Source/ThirdPersonCameraMan/CameraRig.*` — pickup/attach/alignment, switching trigger, SceneCapture configuration
- `Source/ThirdPersonCameraMan/Private/DirectorGameState.cpp` — replicates `ActiveCamera`; OnRep arms capture and shows “Switched to …/none” toasts
- `Source/ThirdPersonCameraMan/ThirdPersonCameraManPlayerController.*` — viewer switches to `ActiveCamera`; drops return viewer to pawn; `Q` drop RPC
- `Source/ThirdPersonCameraMan/ThirdPersonCameraManGameMode.*` — assigns Operator and sets `ActiveCamera`

Logs (optional)
- Rig: `LogDirectorRig`
- GameState: `LogDirectorGS`
- PlayerController: `LogDirectorPC`

Use `log LogDirectorRig VeryVerbose` in the console to increase verbosity if needed.

Repo Layout
- Kept: `Source/`, `Config/`, `.uproject`, scripts
- Ignored: `Content/`, `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, IDE folders

