# OUTLAND Combat System

Combat will be developed as independent gameplay logic before advanced
graphics and effects are added.

Core systems will eventually include:

- health
- damage
- armor
- firearms
- ammunition
- magazines
- reloads
- recoil
- weapon spread
- projectile or hitscan simulation
- death
- respawning

Combat logic should remain testable without rendering.

Example:

A unit test should eventually be capable of verifying that a weapon
with 30 rounds contains 29 rounds after one successful shot without
starting the graphical game.
