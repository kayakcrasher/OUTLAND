# LAN play

Friends on the same Wi-Fi play one match together: **HOME → LAN PLAY - SAME WI-FI**.
It works on desktop and Android. The browser build has no LAN: a web page cannot open UDP sockets, so the button is hidden there.

## Hosting and joining

- **HOST A GAME** opens a lobby. Your game announces itself on the network once a second.
- Everyone else sees it under **GAMES ON THIS NETWORK** and taps it to join. A lobby holds 8 people.
- In the lobby the host picks:
  - **MODE**: Battle Royale, Zombie Survival or Explore.
  - **BOTS**: easy, medium or hard.
- The host then presses **START**. Everyone starts together with the same seed, so they get the same towns, loot, helicopter routes, bots and zone circles.
- Your name is `LUDANTO nn`. Set `OUTLAND_LAN_NAME` to choose your own.

## In the match

- **Other players:**
  - They appear as full bodies with their names overhead.
  - Their positions are smoothed between updates, which arrive 20 times a second.
  - When they drive, you see their car move.
  - You hear their gunfire where it happens.
- **Fighting each other:**
  - Your bullets and grenades hurt other players. The victim's game applies the damage and decides the death.
  - When someone dies, everyone reads it in the kill feed, and the killer gets the kill.
- **Battle Royale bots:**
  - The host runs all 99 bots and sends a snapshot of them (and every moving car) 10 times a second. The other games mirror it.
  - Shooting a bot from a joined game sends the hit to the host. The host decides whether it was a kill and tells the shooter.
  - Bots hunt and shoot every player, not just the host.
- **Shared loot:** taking from a pickup or dropping an item is sent to everyone. A fallen bot's kit gets the same ids in every game, so only one person can loot it.
- **Zone and winning:** the zone closes on the same schedule everywhere. The last one standing wins: no bots and no other players left. ALIVE counts bots plus people, and the HUD shows `LAN n`.
- **No pausing:** a LAN match never stops for one player's menu, bag or result screen.
- **When the host dies:** the host's game keeps running the bots and the zone for everyone still alive.
- **When the match ends:**
  - In Battle Royale, **RESTART** goes back to the lobby for the next match.
  - In Explore and Zombie Survival, **RESTART** puts you back into the same world as the others.
  - **HOME** leaves LAN play.

## How it works

The code is in `src/net` and `src/game/lan`.

- **Transport:** UDP. The host is slot 0, and every message travels through it. Clients send only to the host, and the host relays each message to everyone or to the one slot it is addressed to.
- **Ports:**
  - Discovery beacons go to port `47821` on the broadcast address. That port is shared (`SO_REUSEPORT`), so several games on one machine can listen.
  - Each game socket uses a port the system chooses.
- **Messages:** every message starts with a 12-byte header: `OLAN` magic, version, type, sender slot, target slot and match id. The message types are `LanProtocol.hpp`.
- **Timeouts:** a peer silent for 8 seconds is dropped, and the others see that player leave. If the host goes, everyone else carries on alone and their games start running their own bots.
- **Android:** the manifest asks for `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE` and `CHANGE_WIFI_MULTICAST_STATE`. `GameActivity` holds a Wi-Fi multicast lock while the game is open, because many phones otherwise drop broadcast packets.

## Limits (first version)

- Zombies, Explore residents, police and La Mafio run separately in each game. Only players, Battle Royale bots, loot and grenades are shared.
- The host has authority over bots, but players have authority over themselves. That is fine among friends, not a defence against cheating.
- Every player keeps their own saved bag and money in Explore and Zombie Survival.
- Only the host's own bot kills, and kills by joined players, reach the shared feed. Bot-versus-bot lines show only on the host.

## Testing

- `outland_lan_tests` checks the message format, then runs a host and two clients on loopback through discovery, the lobby, start, relayed play messages, a targeted hit, and leaving.
- Dev builds can drive the lobby unattended:
  - `OUTLAND_DEV_LAN=host` (or `host:explore`, `host:zombie`) hosts and starts once `OUTLAND_DEV_LAN_PLAYERS` (default 2) people are in.
  - `OUTLAND_DEV_LAN=join` joins the first game found.
