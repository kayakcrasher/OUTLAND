# The campaign

The campaign is the game mode that used to be called EXPLORE. You play one man's life on Verda.
The career, trucking, homes, shops and gang wars all happen here.

Code: `game::campaign` (`include/outland/game/campaign/Campaign.hpp`): the story and the slot file.
`game::HomeScreen` has the slot screen and `game::inventory::LootSave` has the slot paths. The
`Renderer` places Uncle Ron and the player, shows the lines and saves the game. Test:
`outland_campaign_tests`.

## The story so far
The player comes home to Verda after years abroad in Eastern Europe. He was born and raised in
Verda City, and several of his family are in **La Verda Mano**, the Green Hand.

1. **The quay at Porto Luma.** A new game starts on the cargo terminal's quay, just off the ship.
   **Uncle Ron** waits by his red pickup ("Find Uncle Ron on the quay"; his name shows over his
   head, and the compass points to him). Ron is a working-class *Esperantisto*, proud his nephew is
   home: "You still remember Esperanto? Ahaha!" His talk on the quay is a short tour of the island
   and a quick tutorial: the port and Verda Transporto, the towns, the ranchers, the Policio, the
   map and USE.
2. **The drive home.** "Get in Uncle Ron's truck": USE beside the truck (or Ron). The screen fades
   ("Uncle Ron drives you home to Verda City...") and comes back at his house in **Okcidenta
   Verda**, the west side of Verda City. His truck is on the drive.
3. **Uncle Ron's house.** He talks straight. His brother **Dario** is a big man in the Green Hand,
   and the player's cousins run with him. Ron doesn't approve, and it saddens him, but he won't
   tell his nephew how to live. Then he gives him the house: *Bonvenon hejmen.* It becomes the
   player's home ("Uncle Ron's house"). It comes furnished, the player can sleep in its bed, and he
   wakes up there after a hospital stay.
4. **From there the game is the player's.** Ron stays at home. USE beside him for a word or two.

The dialogue is in **English** by default. Choose **ESPERANTO** in SETTINGS > Language and Uncle
Ron speaks Esperanto ("Ĉu vi ankoraŭ memoras Esperanton? Ahaha!"). The goals, the drive's caption,
the menus and the slot screen are in Esperanto too. Verda's signs and places are in Esperanto
either way.

USE (or tapping the USE button) shows the next line. If the game is quit during a talk, the talk
starts again when the slot is next loaded.

## Save slots
CAMPAIGN on the home screen opens **three save slots**:
- A used slot shows where the player was, how long they have played and when it was saved. Tap
  it to **continue**.
- An empty slot starts a **new game** on the quay.
- **ERASE** (tap twice) clears a slot: its inventory, career, trucking, territory, builds and
  story.

The files are under `profiles/`. Slot 1 is `explore.loot` (as before slots existed, so older
saves carry on in slot 1, with the story behind them). Slots 2 and 3 are `explore_2.loot` and
`explore_3.loot`. Each slot has its own `.career`, `.career.trucking`, `.territory`,
`.campaign` and `.builds` files.

## Saving
- **Autosave:** every two minutes of free play, and at every step of the story. "Autosaved" shows
  at the bottom right. The career, territory and trucking also save themselves whenever they
  change.
- **KONSERVI LUDON** (SAVE GAME) in the pause menu saves now.
- Quitting to the menu saves too.
- A continued game puts the player back where they were saved: on foot at that spot, or at the
  car's spot if they were driving.

## Dev
- `OUTLAND_DEV_SLOT=N`: CAMPAIGN goes straight into slot N.
- `OUTLAND_DEV_STORY=port|home|skip`: start on the quay, at Uncle Ron's, or with the story done.
