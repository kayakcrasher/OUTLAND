#pragma once
namespace outland::platform {
// A short vibration on phones: the Android app (any device with a vibrator) and browsers that
// support it (Chrome and others on Android; iPhones' Safari does not). Nothing elsewhere.
// strength 0..1 (the Android app varies the motor's force; browsers only the length).
void vibrate(int milliseconds, float strength = 1.0F);
}
