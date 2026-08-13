# GBS-simulateInputPlugin

**Version 4.3.0 — Requires GB Studio ≥ 4.3.0**

A GB Studio engine plugin that allows scripts to inject a scripted sequence of joypad inputs, temporarily overriding the real physical buttons. This can be used to implement cutscene autopilot, tutorial prompts, replay systems, or any scenario where the game needs to drive itself as if the player were pressing buttons.

The plugin adds two events: one to register and start a simulated-input sequence, and one to set the buttons that should be held on each frame within that sequence. The real joypad is completely overridden for the duration of the sequence; a configurable set of physical buttons can interrupt and cancel it at any time.

![image](https://github.com/user-attachments/assets/b0f5bd71-515e-4094-bd51-9259984e0317)
![image](https://github.com/user-attachments/assets/d35be75e-0a0e-462d-9808-fea98e3fe46e)

---

## Table of Contents

1. [Concepts](#concepts)
2. [Project Setup](#project-setup)
3. [Size Limits and Restrictions](#size-limits-and-restrictions)
4. [Events Reference](#events-reference)
5. [Memory Footprint](#memory-footprint)
6. [Bank 0 (HOME) Usage](#bank-0-home-usage)
7. [Changelog](#changelog)

---

## Concepts

### Simulated Input vs Real Input

Every frame, the GB Studio engine reads the joypad hardware into the `joy` global variable and dispatches it to the joypad-event system (`events_update`) and the current scene state (`state_update`). The simulated-input system hooks into the game loop immediately after the VM has finished its script updates, replacing `joy` and the underlying `joypads.joy0` register with a value set by the active sequence script. From the perspective of all engine code that runs after that point, the simulated input is indistinguishable from a real button press.

### Sequence Script and Completion Callback

When `Start simulated inputs` is called, two subscripts are registered:

- **Input sequence** — launched immediately as a concurrent script thread. It runs alongside the rest of the game each frame, advancing one step per game loop. Inside this script, `Set simulated input` events set which buttons are "held" on the coming frame, and `Wait` events advance time.
- **Input sequence completed** — a single-shot script executed when the input sequence thread terminates naturally (reaches its end). It is not executed if the sequence was cancelled.

### Cancel Input

A bitmask of physical buttons can be configured on `Start simulated inputs`. Each frame, if any bit of the real, un-overridden joypad matches the cancel mask, the sequence script is forcibly terminated and the completion callback is **not** run.

---

## Project Setup

1. Copy the plugin folder into your GB Studio project's `plugins/` directory.
2. No additional configuration is required.

---

### How to Use

1. Place a **Start simulated inputs** event where you want the scripted sequence to begin (e.g. the On Init script of a cutscene, or triggered by a trigger/interaction).
2. Configure the **Cancel sequence input** buttons — any of these pressed by the real player will abort the sequence.
3. Inside the **Input sequence** sub-event block, add any combination of:
   - **Set simulated input** — choose which buttons are held starting from this point.
   - **Wait** (frames or seconds) — hold that button state for the desired duration.
   - Repeat as needed to build up the full scripted input timeline.
4. Optionally add events to the **Input sequence completed** sub-event block — these run when the sequence reaches its natural end.

**Example pattern** — move right for 30 frames, then press A:

```
Start simulated inputs  [cancel: B]
  ├─ Input sequence:
  │    Set simulated input  [Right]
  │    Wait  30 frames
  │    Set simulated input  [A]
  │    Wait  10 frames
  │    Set simulated input  []       ← release all buttons
  └─ Input sequence completed:
       ... (optional follow-up script)
```

---

## Size Limits and Restrictions

### Only One Active Sequence at a Time

Only one input sequence can be active at a time. If `Start simulated inputs` is called while a sequence is already running, the existing sequence is **immediately terminated** (without firing the completion callback) before the new one starts.

### Cancel Uses Real (Un-Overridden) Input

The cancel check reads the real joypad before the simulated value is applied, so only genuine physical button presses can cancel the sequence — a simulated button press inside the sequence cannot cancel itself.

### Joypad Override Applies After VM Script Updates

The override is applied in the game loop after script updates have run. Script logic that reads the joypad on the same frame as a **Set simulated input** call sees the simulated value only on the *next* frame.

### Cleared on Scene Change

The simulated input state is fully reset on every scene change. Any in-flight sequence is terminated.

### Preserved Across SceneStackEx Push/Pop

When using the SceneStackExPlugin compatibility variant, simulated input is **not** reset during push or pop scene stack operations. The sequence continues running as if no scene transition occurred.

### Modified Engine File

The plugin patches one stock engine file to hook its update into the game loop, so another plugin that also patches that file needs a merged build or a matching compatibility variant.

---

## Events Reference

### Start Simulated Inputs

**Event ID:** `EVENT_START_SIMULATED_INPUTS`  
**Group:** Input

Registers an input sequence script and a completion callback, then immediately launches the sequence script as a concurrent thread.

| Field | Type | Default | Description |
|---|---|---|---|
| Cancel sequence input | Input (buttons) | A, B | Physical buttons that cancel the sequence when pressed. |
| Input sequence | Events (subscript) | — | The script that runs frame-by-frame to set simulated inputs. Use `Set simulated input` and `Wait` events here. |
| Input sequence completed | Events (subscript) | — | Script executed once when the input sequence reaches its natural end. Not called on cancellation. |

**Notes:**
- If a sequence is already active when this event fires, the old sequence is terminated first.
- The input sequence is a true concurrent script — it runs in parallel with the scene's other scripts.

---

### Set Simulated Input

**Event ID:** `EVENT_SET_SIMULATED_INPUT`  
**Group:** Input

Sets the simulated button state for the current and subsequent frames. Should be placed inside the **Input sequence** subscript of a `Start simulated inputs` event.

| Field | Type | Default | Description |
|---|---|---|---|
| Input | Input (buttons) | *(none)* | The set of buttons to simulate as held. All unselected buttons are treated as released. |

**Notes:**
- The value is held until a subsequent `Set simulated input` changes it.
- An empty selection (no buttons checked) releases all simulated buttons.
- Combine with `Wait` events to hold a button state for a number of frames.

---

## Memory Footprint

Measured against the stock GB Studio **4.3.0-e1** engine by `measure_plugin_memory.js` (per-file SDCC compile with GB Studio's own build flags, at default engine settings; report of 2026-08-13). Figures are this plugin's *delta* versus stock — a file that replaces a stock engine file counts only the difference, which is why a plugin can come out negative. Using the plugin's events additionally compiles a few bytes of GBVM script per call into your project's script banks, on top of the fixed cost below.

| Budget | Cost |
|---|---|
| Bank 0 (HOME) | 0 bytes |
| WRAM | +13 bytes |
| Banked ROM | +456 bytes |

- **Bank 0:** nothing. Every function the plugin adds is compiled into a switchable ROM bank.
- **WRAM:** 13 bytes of input-replay state.
- **Banked ROM:** 456 bytes, net of the stock `core.c` the plugin replaces.
- **Engine WRAM headroom:** a stock GB Studio 4.3.0 project leaves about **854 bytes** of WRAM free (usable engine WRAM is 7,776 bytes at 0xC0A0–0xDF00; the stock engine uses 6,922). With this plugin installed roughly **841 bytes** remain. That does not change with the number of global variables your project defines: the script memory array is a fixed 3,584 bytes at stock engine settings (VM_HEAP_SIZE + VM_MAX_CONTEXTS × VM_CONTEXT_STACK_SIZE = 768 + 16 × 64 words).
- **SRAM:** not used.

---

<!-- BANK0:BEGIN -->
## Bank 0 (HOME) Usage

Bank 0 is the 16 KB non-switchable ROM bank that the GB Studio engine core,
the interrupt handlers and the GBDK runtime all share. Banked ROM is cheap
(add another bank), bank 0 is not, so it is usually the first thing a project
runs out of.

| | Bytes |
|---|---|
| Bank 0 used by this plugin | **0** |

**This plugin costs nothing in bank 0.** Every one of its functions is compiled
into a switchable ROM bank; nothing it adds is resident in bank 0.
<!-- BANK0:END -->

## Changelog

Grouped by the date each change was merged into the official
[gb-studio-plugins](https://github.com/gb-studio-dev/gb-studio-plugins) repository.

Only bug fixes, new features and feature changes are listed. Engine version
bumps, patch regeneration, packaging fixes and documentation edits are omitted.

### 2026-07-19

- Fixed `joy_pressed` not being updated, which broke "just pressed" checks.

### 2025-02-24

- Initial release.
- Fixed the attach input event bug.
