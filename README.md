# GBS-simulateInputPlugin

**Version 4.3.2. Requires GB Studio 4.3.0 or newer.**

Lets a script press buttons for the player. While the sequence runs, the game behaves as if someone
were holding those buttons, and the real controller is ignored.

Use it for a character who walks himself into position at the start of a cutscene, a tutorial that
demonstrates a move, an attract-mode demo on the title screen, or a boss whose second phase takes
the controls away from you for a moment.

Two events do the work. One starts a sequence, the other sets which buttons are held. You choose
which real buttons let the player cancel the sequence.

![image](https://github.com/user-attachments/assets/b0f5bd71-515e-4094-bd51-9259984e0317)
![image](https://github.com/user-attachments/assets/d35be75e-0a0e-462d-9808-fea98e3fe46e)

---

## Table of Contents

1. [Concepts](#concepts)
2. [Project Setup](#project-setup)
3. [Size Limits and Restrictions](#size-limits-and-restrictions)
4. [Events Reference](#events-reference)
5. [FAQ](#faq)
6. [Memory Footprint](#memory-footprint)
7. [Bank 0 (HOME) Usage](#bank-0-home-usage)
8. [Changelog](#changelog)

---

## Concepts

### How the override works

GB Studio reads the controller once per frame and hands the result to everything that cares:
attached input scripts, the player's movement, menus. This plugin swaps that reading for the one
your sequence set, just after scripts have run. Everything downstream sees a real button press.

### The sequence and what follows it

**Start simulated inputs** registers two scripts.

- **Input sequence** starts straight away and runs alongside the rest of the game. Inside it,
  **Set simulated input** chooses which buttons are held and **Wait** decides for how long.
- **Input sequence completed** runs once, when the sequence reaches its end. It does not run if the
  player cancelled.

### Cancelling

**Start simulated inputs** takes a set of buttons that cancel the sequence. Each frame the real
controller is checked against that set, and a match stops the sequence immediately. The completion
script does not run.

---

## Project Setup

1. Copy the plugin folder into your project's `plugins` folder.
2. There is nothing to configure. Compatibility variants ship for the **SceneStackEx** and
   **ConfigLoadSave** plugins, alone or together, and GB Studio picks the right one.

### How to use it

1. Put **Start simulated inputs** where the sequence should begin, for instance a cutscene's On
   Init script or a trigger.
2. Choose the **Cancel sequence input** buttons. Pressing any of them for real stops the sequence.
3. Fill the **Input sequence** block with:
   - **Set simulated input** to choose which buttons are held from this point on;
   - **Wait**, in frames or seconds, to hold that state;
   - as many of each as the sequence needs.
4. Optionally fill the **Input sequence completed** block with what should happen once the sequence
   ends on its own.

**Walk right for half a second, then press A:**

```
Start simulated inputs  [cancel: B]
  Input sequence:
    Set simulated input  [Right]
    Wait  30 frames
    Set simulated input  [A]
    Wait  10 frames
    Set simulated input  []       <- release everything
  Input sequence completed:
    whatever should happen next
```

---

## Size Limits and Restrictions

### One sequence at a time

Calling **Start simulated inputs** while a sequence is running stops the old one immediately,
without running its completion script, and starts the new one.

### Cancelling reads the real controller

The cancel check happens before the override is applied, so only a genuine button press cancels.
A simulated press cannot cancel its own sequence.

### The override lands on the next frame

The override is applied after scripts have run for the frame. A script that reads the controller on
the same frame as a **Set simulated input** sees the new value on the following frame.

### Cleared on scene change

Simulated input resets on every scene change, and any running sequence stops.

### Kept across a SceneStackEx push and pop

With the SceneStackEx variant installed, a push or pop does not reset simulated input. The sequence
carries on as though nothing happened.

### Kept across a load

With the ConfigLoadSave variant installed, a load leaves the sequence running, because this
plugin's state is not part of the save.

### One stock engine file is replaced

The plugin changes the main game loop file to hook itself in, so another plugin that changes the
same file needs a compatibility variant. Two do: SceneStackEx and ConfigLoadSave, and variants ship
for each and for both together.

---

## Events Reference

### Start Simulated Inputs

Group: **Input**.

Registers the sequence and its completion script, then starts the sequence immediately.

| Field | Type | Default | Description |
|---|---|---|---|
| Cancel sequence input | Buttons | A, B | Real buttons that stop the sequence when pressed. |
| Input sequence | Script | none | Runs frame by frame to set the simulated buttons. Use **Set simulated input** and **Wait** here. |
| Input sequence completed | Script | none | Runs once when the sequence reaches its end. Skipped if the player cancelled. |

Notes:

- Starting a sequence while one is running stops the old one first.
- The sequence runs alongside the scene's other scripts.

### Set Simulated Input

Group: **Input**.

Sets which buttons are held from now on. Place it inside the **Input sequence** block of a
**Start simulated inputs** event.

| Field | Type | Default | Description |
|---|---|---|---|
| Input | Buttons | none | The buttons to hold. Anything unticked counts as released. |

Notes:

- The state holds until the next **Set simulated input**.
- Ticking nothing releases every button.
- Follow it with a **Wait** to hold the state for a while.

---

## FAQ

**How do I make the player walk into a room on their own at the start of a cutscene?**
Put **Start simulated inputs** in the scene's On Init script, and inside the sequence hold the
direction you want with **Set simulated input**, then **Wait** long enough to cover the distance.

**Can I build a demo that plays itself on the title screen?**
Yes. Start a long sequence on the title scene and set the cancel buttons to Start and A, so any
real input hands control straight back.

**Does the player keep control while a sequence runs?**
No. The real controller is ignored except for the cancel buttons.

**How do I know when the sequence has finished?**
Put your follow-up in the **Input sequence completed** block. It runs when the sequence ends on its
own, and is skipped when the player cancels.

**How do I tell whether the player cancelled?**
Set a variable at the end of the sequence and check it in the completion script, or set one before
starting and clear it in the completion block. A cancelled sequence never reaches either.

**Can two sequences run at once?**
No. Starting a new one stops the old one immediately, and the old one's completion script does not
run.

**Why does my script not see the simulated button on the same frame?**
The override is applied after scripts have run for the frame, so the new value is visible on the
next one. Add a short **Wait** after **Set simulated input**.

**Can a simulated button press cancel its own sequence?**
No. Cancelling checks the real controller only.

**Does the sequence survive a scene change?**
No. It stops, and simulated input resets. With the SceneStackEx variant a push or pop keeps it
running, and with the ConfigLoadSave variant a load does too.

**Does it clash with other plugins?**
It replaces the main game loop file. SceneStackEx and ConfigLoadSave do too, and compatibility
variants ship for both, alone and together. Another plugin changing that file needs merging by
hand.

**Can I simulate holding two buttons at once?**
Yes. Tick every button you want held in one **Set simulated input**.

---

## Memory Footprint

Measured against the stock GB Studio **4.3.0-e1** engine at default engine settings, report of
2026-08-13. Figures are the difference against a stock project. Each event you use also compiles a
few bytes of script into your project, on top of the fixed cost below.

| Budget | Cost |
|---|---|
| Bank 0 (HOME) | 0 bytes |
| WRAM | +13 bytes |
| Banked ROM | +456 bytes |

- **Bank 0:** nothing. Everything the plugin adds is compiled into a switchable ROM bank.
- **WRAM:** 13 bytes to track the running sequence.
- **Banked ROM:** 456 bytes, after subtracting the stock game loop file the plugin replaces.
- **Engine WRAM headroom:** a stock GB Studio 4.3.0 project leaves about **854 bytes** of WRAM
  free (the engine has 7,776 bytes to work with and uses 6,922 of them). With this plugin
  installed roughly **841 bytes** remain. Adding more global variables to your project does not
  change that figure, because script memory is a fixed 3,584 byte block at stock engine settings.
- **SRAM:** not used.

---

<!-- BANK0:BEGIN -->
## Bank 0 (HOME) Usage

Bank 0 is the 16 KB fixed ROM bank shared by the GB Studio engine core, the
interrupt handlers and the GBDK runtime. Extra banked ROM is cheap to add,
bank 0 is not, so bank 0 is usually the first thing a project runs out of.

| | Bytes |
|---|---|
| Bank 0 used by this plugin | **0** |

**This plugin costs nothing in bank 0.** Everything it adds is compiled into a
switchable ROM bank.
<!-- BANK0:END -->

## Changelog

Grouped by the date each change was merged into the official
[gb-studio-plugins](https://github.com/gb-studio-dev/gb-studio-plugins) repository.

Only bug fixes, new features and feature changes are listed. Engine version bumps, patch
regeneration, packaging fixes and documentation edits are omitted.

### 2026-08-21

- Added compatibility variants for the **ConfigLoadSavePlugin**, which now changes the main game
  loop file too: one for it alone and one for it together with the SceneStackExPlugin.

### 2026-07-19

- Fixed the "just pressed" state not updating, which broke checks for a button press.

### 2025-02-24

- Initial release.
- Fixed the attach input event bug.
