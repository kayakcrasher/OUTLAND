#include "outland/game/vehicles/EngineSynth.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace outland::game::vehicles {

EngineProfile engine_profile(std::string_view d, int engine, int exhaust) {
    EngineProfile p;   // a small four (the hatchback)
    p.pipe = 2.4F; p.muffler = 800;
    if (d == "sedan") { p.cylinders = 6; p.idle = 700; p.redline = 6200; p.body_hz = 95; p.rasp = .35F; p.gain = .9F; p.pipe = 3.0F; p.muffler = 750; }
    else if (d == "wagon") { p.idle = 800; p.redline = 6000; p.body_hz = 105; p.rasp = .4F; p.gain = .85F; p.pipe = 2.8F; p.muffler = 750; }
    else if (d == "muscle") { p.cylinders = 8; p.idle = 650; p.redline = 6000; p.gears = 4; p.lope = .7F; p.body_hz = 68; p.rasp = .35F; p.gain = 1.25F; p.pipe = 3.2F; p.muffler = 1000; }
    else if (d == "green_hand_gt") { p.cylinders = 10; p.idle = 950; p.redline = 8500; p.gears = 6; p.lope = .1F; p.body_hz = 105; p.rasp = .8F; p.gain = 1.15F; p.pipe = 2.6F; p.muffler = 1700; }
    else if (d == "truck") { p.cylinders = 8; p.idle = 650; p.redline = 5200; p.gears = 4; p.lope = .4F; p.body_hz = 72; p.rasp = .3F; p.gain = 1.05F; p.pipe = 3.4F; p.muffler = 850; }
    else if (d == "police" || d == "interceptor" || d == "highway_patrol") {
        p.cylinders = 8; p.idle = 700; p.redline = 6000; p.lope = .3F; p.body_hz = 78; p.rasp = .4F; p.gain = 1.05F; p.pipe = 3.0F; p.muffler = 950;
    } else if (d == "ambulance") { p.cylinders = 8; p.diesel = true; p.idle = 650; p.redline = 3800; p.body_hz = 70; p.rasp = .25F; p.turbo = .3F; p.pipe = 3.4F; p.muffler = 700; }
    else if (d == "van") { p.diesel = true; p.idle = 750; p.redline = 4200; p.body_hz = 90; p.rasp = .3F; p.turbo = .25F; p.gain = .9F; p.pipe = 2.6F; p.muffler = 650; }
    else if (d == "flatbed") { p.cylinders = 6; p.diesel = true; p.idle = 650; p.redline = 3600; p.body_hz = 64; p.rasp = .25F; p.turbo = .4F; p.pipe = 3.6F; p.muffler = 600; p.gain = 1.3F; p.jake = true; p.air_brakes = true; }
    else if (d == "sxs") { p.cylinders = 2; p.idle = 1200; p.redline = 7500; p.cvt = true; p.body_hz = 120; p.rasp = .8F; p.gain = .9F; p.pipe = 1.4F; p.muffler = 1400; }
    // The big rig's straight six breathes out of tall stacks; the bus's out of a long pipe under it.
    else if (d == "semi") { p.cylinders = 6; p.diesel = true; p.idle = 600; p.redline = 2100; p.gears = 10; p.body_hz = 52; p.rasp = .2F; p.turbo = .6F; p.gain = 1.8F; p.pipe = 5.0F; p.muffler = 650; p.jake = true; p.air_brakes = true; }
    else if (d == "bus") { p.cylinders = 6; p.diesel = true; p.idle = 600; p.redline = 2400; p.gears = 6; p.body_hz = 58; p.rasp = .2F; p.turbo = .45F; p.gain = 1.5F; p.pipe = 4.0F; p.muffler = 550; p.air_brakes = true; }
    // The workshop's engines (game::autos::engines): a tune sharpens it; a big block is a V8;
    // turbos whistle, the supercharger whines; the racing engine revs out; bigger diesels pull
    // deeper and spool harder.
    switch (engine) {
    case 1: p.rasp = std::min(1.0F, p.rasp + .2F); p.redline += 400; break;
    case 2: if (!p.diesel) { p.cylinders = 8; p.lope = std::max(p.lope, .6F); p.body_hz = std::min(p.body_hz, 70.0F); p.gain *= 1.15F; } break;
    case 3: p.turbo = std::max(p.turbo, .75F); break;
    case 4: if (!p.diesel) { p.cylinders = 8; p.lope = std::max(p.lope, .45F); p.body_hz = std::min(p.body_hz, 74.0F); } p.whine = .8F; p.gain *= 1.15F; break;
    case 5: p.redline += 1600; p.rasp = std::min(1.0F, p.rasp + .35F); p.lope *= .3F; p.gain *= 1.2F; break;
    case 6: p.turbo = std::max(p.turbo, .55F); p.body_hz *= .92F; break;
    case 7: p.turbo = std::max(p.turbo, .85F); p.body_hz *= .88F; p.gain *= 1.1F; break;
    case 8: p.turbo = 1; p.body_hz *= .82F; p.gain *= 1.2F; break;
    default: break;
    }
    // Exhausts (game::autos::exhausts): louder and deeper; straight pipes crackle.
    if (exhaust == 1) { p.gain *= 1.3F; p.body_hz *= .92F; p.muffler *= 1.6F; }
    if (exhaust >= 2) { p.gain *= 1.7F; p.body_hz *= .86F; p.rasp = std::min(1.0F, p.rasp + .15F); p.pops = true; p.muffler *= 3.5F; }
    if (engine == 5) p.pops = true;
    p.cylinders = std::clamp(p.cylinders, 1, 12);
    p.gears = std::max(1, p.gears);
    return p;
}

float Gearbox::update(float dt, float speed, float throttle, const EngineProfile& p) {
    if (rpm_ <= 0) rpm_ = p.idle;
    speed = std::clamp(std::abs(speed), 0.0F, 1.2F);
    const float gas = std::clamp(throttle, 0.0F, 1.0F);
    float target;
    if (p.cvt) {
        target = p.idle + (p.redline * .82F - p.idle) * std::max(gas * .85F, speed);
    } else {
        // Gear k tops out at (k/n)^0.7 of top speed at 92% of the redline.
        const auto top = [&](int k) { return std::pow(static_cast<float>(k) / static_cast<float>(p.gears), .7F); };
        const auto in_gear = [&](int k) { return p.redline * .92F * speed / top(k); };
        shift_ = std::max(0.0F, shift_ - dt);
        if (shift_ <= 0) {
            const float up = p.redline * (.5F + .38F * gas), down = p.redline * (.26F + .14F * gas);
            if (gear_ < p.gears && in_gear(gear_) > up) { ++gear_; shift_ = p.diesel ? .35F : .2F; }
            else if (gear_ > 1 && in_gear(gear_) < down && in_gear(gear_ - 1) < p.redline * .9F) { --gear_; shift_ = .15F; }
        }
        target = std::max(p.idle, in_gear(gear_));
        // Pulling away: the clutch (the converter) slips, so the revs rise before the wheels catch up.
        if (gear_ == 1) target = std::max(target, p.idle + (p.redline * .45F - p.idle) * gas);
        if (shift_ > 0) target = std::min(target, rpm_);   // off the gas for the change
    }
    target = std::min(target, p.redline * 1.01F);
    const float rate = shift_ > 0 ? 22.0F : target > rpm_ ? 9.0F : 6.0F;
    rpm_ += (target - rpm_) * (1 - std::exp(-dt * rate));
    return rpm_;
}

void EngineSynth::Band::tune(float hz, float q, float rate) {
    const float w = 2 * std::numbers::pi_v<float> * std::clamp(hz, 20.0F, rate * .45F) / rate;
    const float alpha = std::sin(w) / (2 * q), norm = 1 + alpha;
    b0 = alpha / norm;
    a1 = -2 * std::cos(w) / norm;
    a2 = (1 - alpha) / norm;
}

float EngineSynth::Band::run(float x) {
    const float y = b0 * (x - x2) - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = std::isfinite(y) ? y : 0;
    return y1;
}

float EngineSynth::noise() {
    seed_ ^= seed_ << 13; seed_ ^= seed_ >> 17; seed_ ^= seed_ << 5;
    return static_cast<float>(seed_ & 0xFFFFFF) / static_cast<float>(0x7FFFFF) - 1;
}

void EngineSynth::set_profile(const EngineProfile& profile) {
    profile_ = profile;
    const int n = std::clamp(profile.cylinders, 1, static_cast<int>(angle_.size()));
    // Even firing, pushed about by the lope (a cross-plane V8's two banks fire unevenly), and the
    // pulses' strengths uneven with it.
    static constexpr std::array<float, 12> shove{0, .55F, -.35F, .25F, 0, -.55F, .35F, -.25F, .15F, -.15F, .3F, -.3F};
    static constexpr std::array<float, 12> push{1, .7F, 1.15F, .8F, 1.1F, .75F, 1.2F, .85F, 1, .9F, 1.05F, .95F};
    for (int k = 0; k < n; ++k) {
        const auto i = static_cast<std::size_t>(k);
        angle_[i] = (static_cast<float>(k) + profile.lope * shove[i] * .45F) / static_cast<float>(n);
        punch_[i] = 1 + (push[i] - 1) * (.3F + profile.lope);
    }
    next_ = 0;
    phase_ = 0;
    retune_ = 0;
}

void EngineSynth::render(short* out, int frames) {
    std::array<float, 512> chunk{};
    for (int done = 0; done < frames;) {
        const int n = std::min(frames - done, static_cast<int>(chunk.size()));
        render(chunk.data(), n);
        for (int i = 0; i < n; ++i) out[done + i] = static_cast<short>(std::clamp(chunk[static_cast<std::size_t>(i)], -1.0F, 1.0F) * 32000);
        done += n;
    }
}

namespace {
// One exhaust pulse: a quick rise and a longer fall (peak 1 at 22% of its length).
float pulse_shape(float u) {
    const float x = u / .22F;
    return x * x * std::exp(2 * (1 - x));
}
}

void EngineSynth::render(float* out, int frames) {
    const auto& p = profile_;
    const int n = std::clamp(p.cylinders, 1, static_cast<int>(angle_.size()));
    const float rpm = std::max(0.0F, params_.rpm), load = std::clamp(params_.load, 0.0F, 1.0F), red = std::max(1000.0F, p.redline);
    const float rev = rpm / red;
    const float step = rpm / 120 / rate_;   // cycles (two turns) a sample
    const float burst_fall = std::exp(-1 / (rate_ * .0028F)), clatter_fall = std::exp(-1 / (rate_ * .0012F));
    const float blow_fall = std::exp(-1 / (rate_ * .35F)), spool_rate = 1 - std::exp(-1 / (rate_ * .7F));
    const float gain_rate = 1 - std::exp(-1 / (rate_ * .03F)), jake_rate = 1 - std::exp(-1 / (rate_ * .06F));
    const float wind_rate = 1 - std::exp(-2 * std::numbers::pi_v<float> * 500 / rate_);
    const float sigh_fall = std::exp(-1 / (rate_ * .45F));
    const float speed = std::abs(params_.speed);
    // Lifting off at speed: the overrun (and the turbo's blow-off).
    if (last_load_ - load > .4F && spool_ > .4F) blow_ = 1;
    overrun_ = load < .08F && rpm > p.idle * 1.7F ? std::min(1.0F, overrun_ + .2F) : std::max(0.0F, overrun_ - .05F);
    last_load_ = load;
    // The engine brake: off the gas at speed, the rig's jake barks (harder on the brakes).
    const bool barking = p.jake && params_.running && load < .08F && rpm > p.idle * 1.25F && speed > 3;
    const float jake_target = barking ? std::clamp(.65F + .35F * params_.brake, 0.0F, 1.0F) : 0.0F;
    // Air brakes: rolling, then stopped on the brakes - the sigh as they set.
    if (p.air_brakes) {
        if (speed > 2) held_ = 1;
        if (held_ > 0 && speed < .35F) { held_ = 0; sigh_ = 1; ++sighs_; }
    }
    // The pipe: the header's ring (a short pipe, its reflection back up) and the tailpipe's (an open
    // end, the wave comes back inverted). Sound runs about 520 m/s in hot exhaust gas.
    const int header_delay = std::clamp(static_cast<int>(2 * .7F / 520 * rate_), 1, static_cast<int>(header_.size()) - 1);
    const int tail_delay = std::clamp(static_cast<int>(2 * std::max(.5F, p.pipe) / 520 * rate_), 1, static_cast<int>(tail_.size()) - 1);
    const float through = std::clamp((p.muffler - 900) / 3000, 0.0F, .55F);   // how much gets past the muffler unsoftened
    const float cut = std::min(rate_ * .45F, p.muffler * (1 + .6F * load) * (1 + .5F * rev));
    const float muffle = 1 - std::exp(-2 * std::numbers::pi_v<float> * cut / rate_);
    // A pulse is as wide as the blowdown: about a sixth of a turn (wider on a diesel), sharper under load.
    const float cycle = rpm > 1 ? 120 / rpm : 1;
    const float radiate = rate_ / (2 * std::numbers::pi_v<float> * 220);   // unity at 220 Hz
    const float width = std::clamp(cycle * (p.diesel ? .16F : .13F) * (1.15F - .3F * load), .0012F, .03F) * rate_;
    for (int s = 0; s < frames; ++s) {
        if (retune_-- <= 0) {
            retune_ = 64;
            body_.tune(p.body_hz * (1 + .6F * rev), 1.6F, rate_);
            rasp_.tune(650 + rpm * .16F, 1.3F, rate_);
            crack_.tune(p.diesel ? 3300 : 2200, 1.6F, rate_);
            hiss_.tune(2600, 1.1F, rate_);
            squeal_.tune(1050 + 160 * std::sin(whine_phase_ * .37F), 7, rate_);
            roar_.tune(220 + rpm * .12F, .9F, rate_);
            air_.tune(3800, .8F, rate_);
        }
        jake_ += (jake_target - jake_) * jake_rate;
        if (params_.running && step > 0 && n > 0) {
            // The next cylinder's firing point.
            phase_ += step;
            while (phase_ >= 0) {
                const auto i = static_cast<std::size_t>(next_);
                const float at = angle_[i];
                const float following = next_ + 1 < n ? angle_[i + 1] : 1 + angle_[0];
                ++firings_;
                const bool cut_out = params_.limiter && (++skip_ % 3 == 0);
                if (!cut_out) {
                    // Each firing a little different from the last.
                    float a = punch_[i] * (overrun_ > 0 ? .14F + .3F * (1 - overrun_) : .45F + .55F * load) * (1 + .1F * noise());
                    float wide = width * (1 + .06F * noise());
                    if (p.pops && overrun_ > .3F && (noise() > .78F)) { a += 2.6F; clatter_ += 1.6F; ++pops_; }
                    // The engine brake lets the compressed air out at the top: a hard, sharp bark.
                    if (jake_ > .05F) { a += jake_ * 1.8F; wide *= 1 - .5F * jake_; }
                    burst_ += a;
                    if (p.diesel) clatter_ += a * (.7F + .6F * (1 - load));   // the knock, loudest idling
                    for (auto& pulse : pulses_)
                        if (pulse.strength <= 0) { pulse = {0, std::max(2.0F, wide), a}; break; }
                }
                phase_ -= following - at;
                next_ = (next_ + 1) % n;
            }
        }
        // The pressure at the manifold: the pulses in flight.
        float pressure = 0;
        for (auto& pulse : pulses_) {
            if (pulse.strength <= 0) continue;
            pressure += pulse.strength * pulse_shape(pulse.at / pulse.length);
            if (++pulse.at >= pulse.length) pulse.strength = 0;
        }
        // Down the pipe: the header's and the tailpipe's rings.
        const float header = pressure + .3F * header_[static_cast<std::size_t>((header_at_ - header_delay + static_cast<int>(header_.size())) % static_cast<int>(header_.size()))];
        header_[static_cast<std::size_t>(header_at_)] = header;
        header_at_ = (header_at_ + 1) % static_cast<int>(header_.size());
        const float tail = header - .5F * tail_[static_cast<std::size_t>((tail_at_ - tail_delay + static_cast<int>(tail_.size())) % static_cast<int>(tail_.size()))];
        tail_[static_cast<std::size_t>(tail_at_)] = tail;
        tail_at_ = (tail_at_ + 1) % static_cast<int>(tail_.size());
        // The muffler (two gentle stages), and what gets past it raw (sport and straight pipes, the jake).
        muffle1_ += (tail - muffle1_) * muffle;
        muffle2_ += (muffle1_ - muffle2_) * muffle;
        const float raw = std::min(.85F, through + .55F * jake_);
        const float exhaust = muffle2_ + raw * (tail - muffle2_);
        // What comes out of the tailpipe is heard as the change in the flow (the steady push is
        // silent), so slow wide pulses at idle are soft and quick sharp ones at high revs loud.
        dc_out_ = (exhaust - dc_in_) * radiate;
        dc_in_ = exhaust;
        const float body = body_.run(dc_out_) * 1.6F;
        // The intake: a roar that breathes with the firings, under load and high in the revs.
        intake_ = roar_.run(noise() * (.25F + pressure * .5F)) * load * (.2F + .8F * rev) * 1.4F;
        burst_ *= burst_fall;
        clatter_ *= clatter_fall;
        const float rasp = rasp_.run(burst_ * noise()) * p.rasp * (.35F + .9F * load) * 3;
        const float clat = crack_.run(clatter_ * noise()) * 2.2F;
        // Turbo: spools with load and revs; whistles as it spins; blows off when the throttle lifts.
        spool_ += (load * rev - spool_) * spool_rate;
        whistle_ += (1800 + 5600 * spool_) / rate_;
        whistle_ -= std::floor(whistle_);
        const float turbo = p.turbo * (std::sin(whistle_ * 2 * std::numbers::pi_v<float>) * spool_ * spool_ * .07F + hiss_.run(noise()) * blow_ * .6F);
        blow_ *= blow_fall;
        whine_phase_ += rpm / 60 * 4.6F / rate_;
        whine_phase_ -= std::floor(whine_phase_);
        const float whine = p.whine * std::sin(whine_phase_ * 2 * std::numbers::pi_v<float>) * (.015F + .05F * load) * rev;
        // The air brakes' sigh: a burst of air, falling away.
        const float sigh = air_.run(noise()) * sigh_ * (1 - std::exp(-(1 - sigh_) * 40)) * 1.3F;
        sigh_ *= sigh_fall;
        gain_ += ((params_.running ? 1.0F : 0.0F) - gain_) * gain_rate;
        const float engine = (dc_out_ * 1.1F + body * .5F + intake_ * .6F + rasp * .6F + clat * .7F) * gain_ * p.gain + (turbo + whine) * gain_;
        // The road: wind, tyre rumble, the tyres letting go.
        wind_ += (noise() - wind_) * wind_rate;
        rumble_ = rumble_ * .997F + noise() * .02F;
        const float road = wind_ * std::min(1.0F, speed * speed / 1600) * .35F + rumble_ * std::min(1.0F, speed / 20) * .5F +
                           squeal_.run(noise()) * std::clamp(params_.squeal, 0.0F, 1.0F) * 1.4F + sigh;
        out[s] = std::tanh((engine * .55F + road) * 1.2F) * std::clamp(params_.volume, 0.0F, 1.0F) * .8F;
    }
}

} // namespace outland::game::vehicles
