# OUTLAND Project Structure

OUTLAND is an offline 3D open-world combat sandbox written primarily
in C++.

The project is designed around strict separation between engine,
gameplay, world simulation, rendering, platform code, and assets.

This allows gameplay systems to be tested independently from graphics
and allows OUTLAND to target both Linux/X11 development environments
and Android.

---

## src/app

Application startup and lifecycle.

Responsibilities:

- start OUTLAND
- initialize systems
- load configuration
- start the game loop
- perform shutdown

---

## src/engine

Reusable low-level engine systems.

### engine/core

Core runtime functionality.

Examples:

- game loop
- timing
- logging
- configuration
- system initialization

### engine/render

Rendering systems.

Examples:

- renderer
- cameras
- meshes
- models
- materials
- shaders
- lighting

### engine/audio

Audio systems.

Examples:

- weapon sounds
- vehicles
- aircraft
- environmental audio
- music

---

## src/game

Gameplay systems.

### game/player

Player systems.

Examples:

- player state
- movement
- stance
- health
- interaction
- FPS/TPS camera state

### game/combat

Combat rules.

Examples:

- damage
- armor
- health
- death
- hit detection

### game/weapons

Weapon systems.

Examples:

- firearms
- ammunition
- magazines
- recoil
- reload
- ballistics

### game/inventory

Inventory and equipment.

Examples:

- items
- weapon slots
- ammunition storage
- equipment
- loot

### game/vehicles

Ground vehicles.

Examples:

- cars
- trucks
- motorcycles
- vehicle damage
- fuel

### game/aircraft

Aircraft systems.

Examples:

- airplanes
- flight controls
- aircraft damage
- landing

### game/ai

Offline AI players.

Examples:

- perception
- navigation
- combat
- looting
- patrol
- cover
- driving

---

## src/world

Large-world simulation.

### world/terrain

Terrain representation and generation.

### world/chunks

World streaming.

The final OUTLAND world will be divided into chunks so only nearby
areas need to remain active.

### world/towns

Settlements and points of interest.

Examples:

- villages
- towns
- compounds
- military locations
- airfields

### world/spawning

Spawn management.

Examples:

- player spawn
- AI spawn
- loot spawn
- vehicle spawn

---

## src/physics

Physics-facing game systems.

Examples:

- collision
- movement
- gravity
- projectiles
- vehicle physics

---

## src/input

Input abstraction.

Examples:

- keyboard
- mouse
- controller
- Android touch controls

Gameplay code should not depend directly on a specific input device.

---

## platform/linux

Linux and Termux:X11 platform-specific code.

This is the primary development environment.

---

## platform/android

Android-specific platform code.

This will eventually support native APK builds.

---

## maps/training

The first playable OUTLAND environment.

The Training Arena is used to develop and test gameplay systems before
the full open world exists.

---

## maps/world

Future large open-world map data.

Expected content:

- regions
- towns
- roads
- airfields
- landmarks

---

## tests

Automated testing.

### unit

Tests individual gameplay and engine systems.

### integration

Tests multiple OUTLAND systems working together.

The Training Arena will eventually serve as a major integration-test
environment.

---

## Design Rule

Game logic should remain independent from rendering whenever practical.

A weapon should be able to calculate ammunition, damage, reload state,
and firing behavior without requiring a graphical window.

A vehicle should be able to track health, fuel, speed, and state
without requiring a rendered model.

This makes OUTLAND easier to test, optimize, port, and maintain.
