# OUTLAND World System

OUTLAND is designed around a large explorable world.

The world will eventually contain:

- wilderness
- forests
- hills
- roads
- towns
- compounds
- airfields
- landmarks
- vehicles
- aircraft
- AI inhabitants
- loot

The world should not be loaded into memory all at once.

Instead, the map will eventually use a chunk-based streaming system.

Conceptually:

    +-------+-------+-------+
    |       | ACTIVE|       |
    +-------+-------+-------+
    | ACTIVE|PLAYER| ACTIVE |
    +-------+-------+-------+
    |       | ACTIVE|       |
    +-------+-------+-------+

Nearby chunks can contain active simulation.

Distant chunks can be unloaded or represented using lightweight
simulation data.

This architecture is important for Android performance and for
supporting a large map.
