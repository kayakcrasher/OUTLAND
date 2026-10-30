# Engine sound

The engines are synthesised (`game::vehicles::EngineSynth`), not recorded, so each vehicle has
its own voice and it follows what the engine is doing exactly.

## How it sounds

- **The firings.** A four-stroke fires each cylinder once every two turns of the crank. The note's
  pitch is the firing rate: a four at 3,000 rpm fires 100 times a second, a V8 200.
- **The exhaust, modelled the way it works.** Each firing blows a smooth pulse of hot gas into the
  exhaust, as wide as the valve's blowdown (about a sixth of a turn of the crank), so the pulses
  narrow and the note brightens as the revs climb. The pulses run down the pipe, which rings at
  its own lengths: the header's reflection, and the tailpipe's open end sending the wave back
  inverted (each vehicle has its own pipe: 1.4 m on the side-by-side, 5 m up a rig's stacks).
  Then the muffler takes the edge off: a stock one a lot, a sport exhaust less, straight pipes
  hardly at all. What you hear from the tailpipe is the change in the flow, so idling is soft and
  deep and pulling hard is loud and sharp. Every firing is a little different from the last.
- **The intake** roars under load, breathing with the firings; the old version's constant hiss is
  gone (it carried half the sound above 2 kHz; now the weight is in the note).
- **Lope.** A cross-plane V8 fires its banks unevenly; the muscle car and the trucks burble.
- **Diesels clatter and knock**, loudest idling; **turbos** whistle as they spool and blow off when you lift; the
  **supercharged V8** whines.
- **Straight pipes** (and the racing engine) crackle and bang on the overrun.
- **The rev limiter** cuts firings when you hold it at the redline.
- **The big rigs' engine brake.** The semi and the flatbed have a compression-release brake: off
  the gas at speed it barks, each firing a hard sharp blast out of the stacks (louder than pulling,
  harder still on the brake pedal). The semi, the flatbed and the bus **sigh** as their air brakes
  set when they come to a stop.
- **The road:** wind rising with speed, tyre rumble, and **tyre squeal** when the car slides
  sideways, on the handbrake at speed, or spinning its wheels pulling away in something big.

## The gearbox

An automatic turns road speed and throttle into revs. It shifts up as the revs climb (later the
harder you push) and down as they fall; the revs dip through each change. Pulling away, the
converter slips, so the revs rise before the wheels catch up. The side-by-side has a belt (CVT):
its revs ride the throttle.

## Each vehicle

| Vehicle | Engine | Gears |
|---|---|---|
| Hatchback, Familio | small four | 5 |
| Sedano | six | 5 |
| Tempesto | V8, a big lope | 4 |
| Mano GT | V10, revs to 8,500 | 6 |
| Pickup | V8 | 4 |
| Police cars | V8 | 5 |
| Ambulance | V8 diesel, turbo | 5 |
| Furgono | four-cylinder diesel, turbo | 5 |
| Platformo | six-cylinder diesel, turbo | 5 |
| Side-by-side | twin, belt drive | CVT |
| Big rig, bus | six-cylinder diesel, big turbo | 10, 6 |

The workshop changes it (`game::autos`): a tune sharpens it and revs higher; the big block and
the supercharger make it a V8; the turbo whistles; the racing engine revs 1,600 rpm further and
pops; bigger diesels pull deeper and spool harder. The sport exhaust is louder and deeper, straight
pipes louder still.

## In the game

`VehicleAudio` keeps two engine streams (22,050 Hz mono, fed 1,536 frames at a time): the
player's vehicle, and the nearest other vehicle on the move within 40 m (fading with distance;
its driver's throttle comes from its autopilot). The recorded starter, ignition off, horn, doors
and parking brake still play over them. Tests: `tests/unit/vehicles/test_engine_synth.cpp`
(firing rates, loudness, the parts, the gearbox) and
`tests/integration/vehicles/test_vehicle_audio.cpp` (the streams).
