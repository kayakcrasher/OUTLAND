#pragma once

namespace outland::engine {

class GameTime {
public:
    void update(double delta_seconds);

    [[nodiscard]]
    double delta_seconds() const;

    [[nodiscard]]
    double total_seconds() const;

private:
    double delta_seconds_{0.0};
    double total_seconds_{0.0};
};

}
