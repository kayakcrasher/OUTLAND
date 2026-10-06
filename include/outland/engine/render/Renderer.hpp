#pragma once

namespace outland::engine {

class Renderer {
public:
    Renderer() = default;

    bool initialize(
        int width,
        int height
    );

    void run();

    void shutdown();

    [[nodiscard]]
    bool initialized() const;

private:
    bool initialized_{false};

    int width_{1280};
    int height_{720};
};

}
