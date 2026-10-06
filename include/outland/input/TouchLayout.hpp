#pragma once

namespace outland::input {

struct TouchElementLayout {
    float x{0.0F};
    float y{0.0F};
    float scale{1.0F};
    float opacity{0.50F};
};

struct TouchLayout {
    TouchElementLayout movement{
        0.60F,
        0.76F,
        1.20F,
        0.30F
    };

    TouchElementLayout look{
        0.76F,
        0.76F,
        1.20F,
        0.25F
    };

    TouchElementLayout jump{
        0.91F,
        0.58F,
        1.0F,
        0.40F
    };

    TouchElementLayout view{
        0.91F,
        0.79F,
        0.85F,
        0.35F
    };

    TouchElementLayout fire{
        0.91F,
        0.34F,
        1.10F,
        0.40F
    };

    TouchElementLayout aim{
        0.80F,
        0.34F,
        0.90F,
        0.35F
    };

    float look_sensitivity{2.25F};

    void reset() {
        *this = TouchLayout{};
    }
};

}
