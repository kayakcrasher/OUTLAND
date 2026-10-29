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
