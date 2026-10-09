# OUTLAND
Offline 3D open-world combat sandbox built in C++

Requires C++20 and **raylib 6.x** (validated against 6.0.0). The previous raylib
5.5 model/skeleton API is no longer supported. Use a fresh build directory when
changing raylib versions.

```sh
cmake -S . -B build/dev-raylib6 -DOUTLAND_DEV_TOOLS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/dev-raylib6 -j2
ctest --test-dir build/dev-raylib6 --output-on-failure
```

DEV vehicle driving, map building, controls, save compatibility and Termux steps:
[vehicle and Creator handoff](docs/vehicles-creator-raylib6-handoff.md).
