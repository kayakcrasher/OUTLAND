#pragma once
namespace outland::platform {
// Called after raylib creates its native window, before any shared asset loaders.
bool initialize();
void begin_frame();
bool save_requested();
void flush_saves();
}
