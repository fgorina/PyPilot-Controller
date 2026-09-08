# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---
## Aims of the project

We are modifying the progject to run on a M5Though instead of a M5Dial
because M5Dial is prono to failing when outside.

Given the big refactor we are changing the structure to be more similar to new projecta as MetoGNSS

So we will use Screen and M5Button 

All references to Calypso anemometer should be removed. Also SignalK won't be needed
as we connect directly to the PyPilot

We don't do calibration, just navigation sop we should be able (MenuScreen) to

- Change Mode (Compass, GPS, Wind, TrueWind)
- Start/Stop navigation 
- Change target by 1 and 10 degrees
- Start tacking in either direction.

Changing target + Start tacking is the same screen.

If needed we may build different screens for different modes.

We may need to add a new AlertScreen to show alertsand an infoScreen for showing other data.

We must add support for external physical buttons. The screen when wet is difficut to use so:

- One stop button. When clicked or if it is on it stops autopilot and ensures it is not
started by another device, if the other device starts it it should stop it. Works like a 
industrial machine emergency stop. I think one of the kmost dangerous things is AP not letting
you override it.

- One Start button. when pushed and if stop button is not activated, will start the autopilot.
For the moment start button will always start in Compass mode with current heading!!!.

Independent of this, the AP may me controlled from the touch screen by the software buttons.

Also AP is controlled from the BLE Interface.

Of course PyPilot may be engaged by another device and the screen should reflect this. 

data_model + pypilot_parse will be in just one class State

We will try to create all classes as .h/.cpp, preferably use c++ strings that Arduino String unless needed.

README.md has information abut details of the project and should be read when beginning a session.

---

## Build

```bash
pio run           # build
pio run -t upload # flash
```

- Target: `m5stack-cores3` (defined in `platformio.ini`)
- Partition table: `default_16MB.csv`
- Display driver: LovyanGFX — **avoid single-argument `setTextColor()`** (always pass background colour as second argument)
- PSRAM is required and assumed available
- Entry point: `src/main.cpp`

---

## Library Structure

```
lib/
  M5Unified  — M5 Library for device 🔒 CLOSED
  ArduinoJson - For parsing JSON
  ArduinoWebsockets - For accessing SignalK
```

**Do not modify closed/settled libraries unless explicitly instructed.**




## Screen (`Screen.h` / `Screen.cpp` / `M5Button.h`). CLOSED

Main UI element. 
Do not touch. consider it CLOSED.
If need to modify first propose idea and ask.

Same is true for M5Button although we will probably chage it at some moment. Wait istructions.


## UI (Based in Screen)

PilotScreen (.cpp, .h) main Interfaces (to be designed)
RudderScreen (.cpp, .h) Shows Rudder mode
MenuScreen (.cpp, .h) Start and wait screen tos elect pilot mode.

## Configuration
First time it runs gets name as "PYPILOT_" + rtandom between 100 and 1000. Will be deviceName and initial ssid 
Defined in AppConfig.h ()
Stored in Preferences
Configured via web server in net_webserver (.h, .cpp) - Must adapt
When ssid is not configured creates a WiFi with ssid AppConfig.deviceName
PyPilot is located by mDNS but xdefault is OK for Yamato

## Reading data (Sensors.h, Sensors.cpp)
All device / sensor reading code should be in Sensors

## Communications
Creates a BLE Server so the pilot may be controlled by an AppleWatch through BLE.



---

## Key Design Decisions (do not revert without discussion)

- **Reduce dynamic allocation at runtime** — State must be sufficient.
- **SI units in State, always** — unit conversion is a display/formatter concern only.
- **LovyanGFX `setTextColor()` must always have two arguments, accessed from M5Unified/M5GFX** .
- **Config on Preferences **.
