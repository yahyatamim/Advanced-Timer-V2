# GEMINI.md: Advanced Timer V2

This document provides a comprehensive overview of the Advanced Timer V2 project for Gemini. It covers both the current proof-of-concept (PoC) implementation and the planned evolution toward a production-grade firmware.

## Project Overview

This project is a sophisticated, field-configurable smart controller firmware, known as "Advanced Timer." It is designed for the **ESP32** platform and is developed using **PlatformIO** with the **Arduino framework**. The project is currently in a functional PoC state, with a formal plan to rewrite the firmware for production-readiness.

The core purpose is to provide a "no-code" environment for defining complex control logic. This is achieved through a web-based configuration portal served from the device. The central concept is the **LogicCard**, a parameter-driven virtual component that creates PLC-like behavior without writing code.

### Key Technologies & Architecture (Current PoC)

*   **Firmware:** C++ on the Arduino framework, largely contained in `src/main.cpp`.
*   **Build System:** PlatformIO.
*   **Hardware:** `esp32doit-devkit-v1` or compatible ESP32 board.
*   **Architecture:** A strict **dual-core architecture** is enforced using FreeRTOS tasks.
    *   **Core 0 (Kernel):** Runs the deterministic, real-time logic engine. It handles LogicCard evaluation, timing, and I/O.
    *   **Core 1 (Portal/Network):** Manages all non-deterministic tasks: WiFi, web server, and WebSockets.
*   **Frontend:** A vanilla JavaScript single-page application stored in `/data` and served from the ESP32's **LittleFS** filesystem.
*   **Communication:** A combination of a **HTTP REST API** for configuration management and **WebSockets** for real-time state streaming and commands.
*   **Configuration:** Device logic is stored as versioned JSON files on the device's filesystem.

## Production Firmware Rewrite (V2)

The project is undergoing a formal, phased rewrite to create a production-ready firmware. This effort is detailed in `docs/legacy/firmware-rewrite-foundations.md`, `docs/legacy/production-firmware-kickoff-plan.md`, and is specified in the new authoritative contract, **`requirements-v2-contract.md`**.

The key goals of the V2 rewrite are to improve reliability, testability, security, and architectural clarity.

### V2 Architecture and Design Principles

*   **Layered Architecture:** The monolithic `src/main.cpp` will be refactored into a clean, layered architecture with strict boundaries:
    *   `src/kernel/`: Core deterministic logic, scan scheduler.
    *   `src/runtime/`: Snapshots, metrics, and fault states.
    *   `src/control/`: Command validation and dispatch.
    *   `src/storage/`: Power-loss-safe configuration persistence and rollback.
    *   `src/portal/`: HTTP/WebSocket transport layer.
    *   `src/platform/`: Hardware Abstraction Layer (HAL) for board-specific primitives.
*   **Expanded Card Families:** The V2 contract formalizes two new card families:
    *   **`MATH`:** For deterministic numeric computations (e.g., add, scale, clamp).
    *   **`RTC`:** For real-time clock and schedule-based logic.
*   **Integrated Behavior:** Timers and counters are not standalone cards. Instead, this functionality is integrated directly into the `DO`/`SIO` (timer) and `DI` (counter) card families.
*   **Variable Assignment:** A formal system for binding card parameters to runtime variables (e.g., the output of a `MATH` or `AI` card) instead of only static constants. This is managed via a validated, acyclic dependency graph.
*   **Structured Logging:** A macro-based logging framework will be used instead of raw `Serial.print()` statements. This provides compile-time control over log levels (`DEBUG`, `INFO`, `WARN`, `ERROR`), allowing for verbose output during development and zero performance overhead in production builds, as unused log statements are compiled out entirely.
*   **Test-Driven Development:** The V2 firmware will be governed by a comprehensive suite of acceptance tests, unit tests, and hardware-in-the-loop (HIL) tests running in a CI/CD pipeline.
*   **Reliability & Security:** The rewrite will add robust fault-containment (watchdogs, reboot-reason logging), power-loss-safe transactional config commits, and a security model with roles (`Viewer`, `Operator`, `Admin`) and authenticated access for sensitive operations.

## Building and Running

This is a PlatformIO project. The following commands are standard.

**Note:** As the V2 refactor progresses, the source code will move from a single `src/main.cpp` into the layered subdirectories mentioned above. The build commands will remain the same.

### 1. Build Firmware

To compile the firmware:

```sh
platformio run
```

### 2. Upload Firmware

To compile and upload the firmware to the connected ESP32 device:

```sh
platformio run --target upload
```

### 3. Upload Filesystem Image

The web portal UI files in `data/` must be uploaded to the device's LittleFS filesystem.

```sh
platformio run --target uploadfs
```

## Development Conventions

*   **Authoritative Contracts:**
    *   For the **current PoC**, `README.md` is the primary contract.
    *   For the **V2 rewrite**, `requirements-v2-contract.md` is the new, authoritative source of truth.
*   **Strict Separation:** The dual-core boundary is non-negotiable. The kernel must remain deterministic.
*   **Configuration as Data:** Logic is defined by JSON configuration files, not user code.
*   **API-Driven:** All external interactions are handled via the documented HTTP and WebSocket APIs.
*   **Incremental Refactor:** The transition from PoC to V2 is a phased process, as outlined in the kickoff plan. This allows for continuous integration and testing without a "big bang" switch.
