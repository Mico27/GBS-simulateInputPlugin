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
3. [How to Use](#how-to-use)
4. [Technicalities and Restrictions](#technicalities-and-restrictions)
5. [Events Reference](#events-reference)
6. [Inner Workings](#inner-workings)
7. [Memory Footprint](#memory-footprint)

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

## How to Use

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

## Technicalities and Restrictions

### Only One Active Sequence at a Time

Only one input sequence can be active at a time. If `Start simulated inputs` is called while a sequence is already running, the existing sequence is **immediately terminated** (without firing the completion callback) before the new one starts.

### Cancel Uses Real (Un-Overridden) Input

The cancel check reads `joy` before the simulated value is applied. This means only genuine physical button presses can cancel the sequence — a simulated button press inside the sequence cannot cancel itself.

### Joypad Override Applies After VM Script Updates

`simulate_input_update()` runs in the game loop after `script_runner_update()` completes. Script logic that reads `joy` during the same VM frame as a `Set simulated input` call will see the simulated value only on the *next* frame.

### Cleared on Scene Change

The simulated input state is fully reset on every scene change (`EXCEPTION_CHANGE_SCENE`). Any in-flight sequence is terminated.

### Preserved Across SceneStackEx Push/Pop

When using the SceneStackExPlugin `engineAlt` variant, simulated input is **not** reset during push or pop scene stack operations. The sequence continues running as if no scene transition occurred.

### Modified Engine File

The plugin replaces `engine/src/core/core.c` to integrate the initialisation and update calls into the game loop. It also adds two new engine files: `simulate_input.c` and `simulate_input.h`.

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

## Inner Workings

### New Engine Files

**`simulate_input.h`** declares two public functions:

```c
void simulate_input_init(UBYTE preserve) BANKED;
void simulate_input_update(void) BANKED;
```

**`simulate_input.c`** contains all state and logic:

```c
script_event_t input_sequence_event;       // tracks the running sequence thread
script_event_t input_sequence_completed_event;  // completion callback registration
UBYTE cancel_input_mask;                   // physical-button cancel bitmask
UBYTE sim_frame_joy;                       // current simulated joypad byte
UBYTE last_sim_joy;                        // previous frame's simulated joypad byte
```

The joypad bitmask encoding (same constants used in both the JS event and the C runtime):

| Button | Bit |
|---|---|
| Right | 0x01 |
| Left | 0x02 |
| Up | 0x04 |
| Down | 0x08 |
| A | 0x10 |
| B | 0x20 |
| Select | 0x40 |
| Start | 0x80 |

### `simulate_input_init`

```c
void simulate_input_init(UBYTE preserve) BANKED {
    if (preserve) {
        input_sequence_completed_event.handle = 0;
        input_sequence_event.handle = 0;
    } else {
        cancel_input_mask = 0;
        input_sequence_completed_event.handle = 0;
        input_sequence_completed_event.script_addr = 0;
        input_sequence_completed_event.script_bank = 0;
        input_sequence_event.handle = 0;
        input_sequence_event.script_addr = 0;
        input_sequence_event.script_bank = 0;
        sim_frame_joy = 0;
    }
}
```

Called with `preserve = FALSE` on game reset and on scene change — clears all state including the registered script pointers and the cancel mask. Called with `preserve = TRUE` to clear only the active thread handles without discarding the registered scripts.

### `simulate_input_update`

Called once per frame, after the VM has finished processing but before state updates:

```c
void simulate_input_update(void) BANKED {
    if (!input_sequence_event.script_bank) return;
    if ((input_sequence_event.handle & SCRIPT_TERMINATED) != 0) {
        // sequence finished naturally — fire completion callback
        input_sequence_event.script_addr = 0;
        input_sequence_event.script_bank = 0;
        input_sequence_event.handle = 0;
        if (input_sequence_completed_event.script_addr) {
            script_execute(input_sequence_completed_event.script_bank,
                           input_sequence_completed_event.script_addr, 0, 0);
        }
    } else if (joy & cancel_input_mask) {
        // physical cancel button pressed — kill the sequence
        script_terminate(input_sequence_event.handle);
    } else {
        // sequence is running — apply simulated joypad
        last_joy = last_sim_joy;
        joypads.joy0 = joy = sim_frame_joy;
        if ((joy ^ last_joy) & INPUT_DPAD) {
            recent_joy = ((joy & ~last_joy) & INPUT_DPAD);
        }
        last_sim_joy = sim_frame_joy;
    }
}
```

The `SCRIPT_TERMINATED` flag is polled on the handle written by `script_execute`. When set, the sequence has ended. If the cancel mask matches, `script_terminate` forcibly ends the thread and on the next frame `SCRIPT_TERMINATED` will be set, but the completion check will have already been bypassed. The `recent_joy` update replicates the engine's D-Pad edge-detection logic so that movement events that depend on direction-change triggers work correctly with simulated input.

### `vm_attach_simulate_input`

The native function called by `Start simulated inputs`:

```c
void vm_attach_simulate_input(SCRIPT_CTX * THIS) OLDCALL BANKED {
    cancel_input_mask = *(int8_t*)VM_REF_TO_PTR(FN_ARG0);
    uint8_t input_sequence_bank    = *(uint8_t *) VM_REF_TO_PTR(FN_ARG1);
    void*   input_sequence_ptr     = *(void**)    VM_REF_TO_PTR(FN_ARG2);
    uint8_t completed_bank         = *(uint8_t *) VM_REF_TO_PTR(FN_ARG3);
    void*   completed_ptr          = *(void**)    VM_REF_TO_PTR(FN_ARG4);

    if ((input_sequence_event.handle & SCRIPT_TERMINATED) == 0) {
        script_terminate(input_sequence_event.handle);
    }
    input_sequence_event.script_bank = input_sequence_bank;
    input_sequence_event.script_addr = input_sequence_ptr;
    input_sequence_completed_event.script_bank = completed_bank;
    input_sequence_completed_event.script_addr = completed_ptr;
    if (input_sequence_event.script_bank) {
        script_execute(input_sequence_event.script_bank,
                       input_sequence_event.script_addr,
                       &input_sequence_event.handle, 0);
    }
}
```

If the previous sequence is still alive its thread is terminated before the new one is started. The `handle` written by `script_execute` is what `simulate_input_update` polls each frame to detect termination or apply cancellation.

### `vm_set_simulated_input`

The native function called by `Set simulated input`:

```c
void vm_set_simulated_input(SCRIPT_CTX * THIS) OLDCALL BANKED {
    last_sim_joy = sim_frame_joy;
    sim_frame_joy = *(int8_t*)VM_REF_TO_PTR(FN_ARG0);
}
```

Updates `sim_frame_joy` with the encoded button bitmask. The old value is saved to `last_sim_joy` so `simulate_input_update` can compute direction-change edges correctly on the next frame.

### Game Loop Integration (`core.c`)

`simulate_input_update` is called in the main game loop in the idle branch, after the VM completes and before the joypad-event and state-update functions:

```c
if (!VM_ISLOCKED()) {
    simulate_input_update();
    if (joy != 0) events_update();
    if (!pause_state_update) state_update();
    ...
}
```

This ordering ensures that when the simulated joypad value is applied to `joy`, both `events_update` (which fires on-input trigger scripts) and `state_update` (which moves actors and handles player input) see the simulated buttons rather than the real ones.


---

## Memory Footprint

Measured against the stock GB Studio **4.3.0-e1** engine (per-file SDCC compile with GB Studio's build flags, default engine settings). Values are the plugin's *delta* versus the stock engine; DMG build, with CGB noted where it differs. ROM cost lands in banked ROM (GB Studio's autobanker spreads it across switchable banks); using the plugin's events additionally compiles a few bytes of GBVM script per call into your project's script banks.

| | Cost |
|---|---|
| WRAM | +13 bytes |
| ROM | +442 bytes |

- **WRAM:** 13 bytes of input-replay state in `simulate_input.c`.
- **Engine WRAM headroom:** the stock GB Studio 4.3.0 engine leaves about **854 bytes** of WRAM free (usable engine WRAM is 7,776 bytes at 0xC0A0–0xDF00; the stock engine uses 6,922 bytes). With this plugin installed roughly **841 bytes** remain. This figure does not depend on how many global variables your project defines: the script memory array has a fixed size of VM_HEAP_SIZE + (VM_MAX_CONTEXTS × VM_CONTEXT_STACK_SIZE) words — 768 + 16 × 64 = 1,792 words (3,584 bytes) with stock engine settings.
- **SRAM:** not used.
