# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DeskHog is an embedded firmware for the ESP32-S3 Reverse TFT Feather - a palm-sized developer toy with a 240x135 color display, WiFi, battery operation, and an expandable card-based UI system. It displays PostHog analytics insights and hosts games/apps.

## Development Commands

### Build and Flash
```bash
# Build the firmware
pio run

# Upload to device (ensure device is connected via USB)
pio run --target upload

# Clean build artifacts
pio run --target clean

# Erase flash and upload (use when partition issues occur)
pio run --target erase
pio run --target upload

# Monitor serial output
pio device monitor

# Build and upload in one command
pio run --target upload && pio device monitor
```

### Testing (currently disabled but infrastructure exists)
```bash
# Run tests (requires uncommenting test_native in platformio.ini)
pio test --environment test_native
```

### Asset Generation (runs automatically on build)
```bash
# Convert PNGs to C arrays (sprites)
python3 png2c.py

# Convert TTF fonts to C arrays
python3 ttf2c.py

# Inline HTML portal assets
python3 htmlconvert.py
```

### Device Reset Sequences
- **Bootloader mode** (for re-flashing): Hold ▼ (D0) → Press Reset → Release ▼
- **Power off**: Hold ● + ▼ for 2 seconds (enters deep sleep)
- **Wake from sleep**: Press Reset button

## Architecture & Critical Constraints

### Core/Task Isolation (CRITICAL)
The firmware uses FreeRTOS with strict core isolation to prevent crashes:

**Core 0 (Protocol CPU)**:
- WiFi operations
- Web portal server
- PostHog API client
- NeoPixel LED control
- **NEVER touch UI objects from Core 0**

**Core 1 (Application CPU)**:
- LVGL graphics library tick
- UI drawing and updates
- Input handling
- **ALL UI updates MUST happen here**

### Cross-Core Communication
Use `EventQueue` for all cross-core messaging:
```cpp
// From Core 0 to UI task
EventQueue::getInstance().push({EventType::INSIGHT_UPDATE, data});

// UI task processes events
Event event;
if (EventQueue::getInstance().pop(event)) {
    // Handle event on Core 1
}
```

### Memory Management
- **PSRAM enabled**: Use for large allocations
- **Flash storage limited**: Aggressively optimize assets
- **Web portal budget**: Max 100KB (currently ~18KB)
- **Display buffer**: 240x135 pixels full screen height

### Key Design Patterns

1. **EventQueue Pattern**: All cross-core communication through thread-safe queue
2. **Card Stack Navigation**: UI organized as navigable card stack
3. **Factory Pattern**: Dynamic card registration via factory functions
4. **InputHandler Base**: For cards needing update loops (games/animations)

## Card System Development

### Adding a New Card Type

1. Add to `CardType` enum in `src/ui/CardController.h`
2. Create card class inheriting from base or `InputHandler` for games
3. Register in `CardController::initializeCardTypes()`:
```cpp
CardDefinition myDef;
myDef.type = CardType::MY_CARD;
myDef.name = "My Card";
myDef.factory = [this](const String& config) -> lv_obj_t* {
    MyCard* card = new MyCard(screen);
    return card->getCard();
};
registerCardType(myDef);
```

### Game Cards (with Update Loop)
Inherit from `InputHandler` for frame updates:
```cpp
class GameCard : public InputHandler {
    bool handleButtonPress(uint8_t button) override;
    bool update() override; // Called ~60 FPS when card is active
    void prepareForRemoval() override;
};
```

## Key Components

| Component | Purpose | Core Constraint |
|-----------|---------|----------------|
| `ConfigManager` | Persistent storage (NVS) | Either core |
| `EventQueue` | Cross-core messaging | Thread-safe |
| `CardController` | Card lifecycle management | Core 1 only |
| `CardNavigationStack` | UI transitions/animations | Core 1 only |
| `PostHogClient` | API requests | Core 0 only |
| `InsightParser` | Response parsing | Core 0 only |
| `CaptivePortal` | Web server/config UI | Core 0 only |
| `DisplayInterface` | TFT control | Core 1 only |

## Development Guidelines

### Code Style
- Use `snakeCase` for variables
- Separation of concerns: No network code in UI components
- Follow existing EventQueue patterns for messaging
- Clean up "LLM slop" (unused variables, odd delays)

### Before Writing Code
1. Consider existing project context
2. Propose a plan for implementation
3. Verify core/task constraints are respected
4. Ensure UI updates only happen on Core 1

### Common Pitfalls to Avoid
- **Never** call UI functions from Core 0
- **Never** create LVGL objects outside UI task
- **Always** use EventQueue for cross-core data
- **Always** check partition issues if flash behavior is odd
- **Avoid** creating new files unless absolutely necessary
- **Prefer** editing existing files over creating new ones

## Project Structure

```
src/
├── main.cpp                    # Entry point, task creation
├── hardware/                   # Hardware interfaces (Core 0/1 specific)
├── ui/                         # UI components (Core 1 only)
│   ├── cards/                  # Individual card implementations
│   └── renderers/              # PostHog data visualizers
├── posthog/                    # API client (Core 0 only)
└── config/                     # Configuration structures

include/
├── sprites/                    # Auto-generated from PNG files
├── fonts/                      # Auto-generated from TTF files
└── html_portal.h              # Auto-inlined web portal
```

## Platform Configuration

- **Board**: Adafruit ESP32-S3 Reverse TFT Feather
- **Display**: 240x135 ST7789 TFT
- **Framework**: Arduino + ESP-IDF
- **Graphics**: LVGL 9.2.2
- **Build**: PlatformIO with custom pre-build scripts
- **Partition**: Split for OTA updates (see `partitions.csv`)