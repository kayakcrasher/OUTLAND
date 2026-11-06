# Word gets around

The island talks. Every headline (a town changing hands, a ballgame, a race on the ring, a feast)
is heard by the island. What the player does is heard too, with the car they did it in: "somebody
in a racing red Verda Tempesto ran a man down in Roka and never stopped".

- **Talking to people:** stand by a resident and press USE ("E - Talk"). They give their name and
  pass on a story.
- **Which story:** stories about the player come first, then the town's own news, then the
  freshest. Each story is passed on at most three times. Stories are forgotten after three island
  days.
- **Small talk:** with nothing new, people talk about the next feast, the night races or the
  league leader.
- **Ron hears it too:** a story about the player can bring a call from Uncle Ron (see
  campaign.md).

Code: `game::culture::Gossip` (`include/outland/game/culture/Gossip.hpp`). Test:
`tests/unit/culture/test_gossip.cpp`.

## Messages on screen

Island headlines are not pushed at a new player. The pause menu's **Messages** setting
(`GameSettings::News`, saved as `news`) decides what shows on screen:

- **AUTO** (default): your own business always shows (your street race, your turf, your family,
  your crew, the siege). Island news (feasts, the baseball league) shows only after 3 hours played
  (`GameSettings::news_after_hours`). Faction news shows only to the people in it: a gang war to
  gang members, a raid to the police, the truckers' news to the brotherhood. Ron and Dario only
  phone once you've played 3 hours or have taken a side (gang, police, truckers).
- **ONLY MINE**: just your own business; no island news, no phone calls.
- **OFF**: no messages at all.

Anything not shown on screen still goes into the gossip, so residents can tell you about it.
Code: the `show` / `tell` lambdas in `src/engine/render/Renderer.cpp`.
