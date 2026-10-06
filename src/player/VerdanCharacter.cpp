#include "outland/player/VerdanCharacter.hpp"

#include <raylib.h>
#include <raymath.h>

#include <cmath>

namespace outland::player {

namespace {

Vector3 rotate_offset(
    const Vector3 offset,
    const float yaw
) {
    const float s =
        std::sin(yaw);

    const float c =
        std::cos(yaw);

    return {
        offset.x * c +
            offset.z * s,

        offset.y,

        -offset.x * s +
            offset.z * c
    };
}

Vector3 world_position(
    const Vector3 origin,
    const Vector3 offset,
    const float yaw
) {
    return Vector3Add(
        origin,
        rotate_offset(
            offset,
            yaw
        )
    );
}

void draw_limb(
    const Vector3 start,
    const Vector3 end,
    const float thickness,
    const Color color
) {
    const Vector3 center =
        Vector3Scale(
            Vector3Add(
                start,
                end
            ),
            0.5F
        );

    const float length =
        Vector3Distance(
            start,
            end
        );

    /*
     * Capsule gives us much more natural
     * arms and legs than rectangular blocks.
     */
    DrawCapsule(
        start,
        end,
        thickness,
        8,
        8,
        color
    );

    (void)center;
    (void)length;
}

}

void VerdanCharacter::draw(
    const Vector3 feet_position,
    const float yaw,
    const float movement_amount,
    const float animation_time
) {
    const Color skin{
        181,
        142,
        111,
        255
    };

    const Color shirt{
        76,
        91,
        63,
        255
    };

    const Color pants{
        63,
        67,
        62,
        255
    };

    const Color boots{
        48,
        42,
        36,
        255
    };

    const Color vest{
        82,
        78,
        61,
        255
    };

    const Color hair{
        58,
        44,
        33,
        255
    };

    const float walk =
        std::sin(
            animation_time * 8.5F
        ) *
        0.28F *
        movement_amount;

    const float opposite_walk =
        -walk;

    // ========================================================
    // BODY REFERENCE POINTS
    // ========================================================

    const Vector3 hips =
        world_position(
            feet_position,
            {
                0.0F,
                0.90F,
                0.0F
            },
            yaw
        );

    const Vector3 chest =
        world_position(
            feet_position,
            {
                0.0F,
                1.42F,
                0.0F
            },
            yaw
        );

    const Vector3 neck =
        world_position(
            feet_position,
            {
                0.0F,
                1.67F,
                0.0F
            },
            yaw
        );

    const Vector3 head =
        world_position(
            feet_position,
            {
                0.0F,
                1.82F,
                0.0F
            },
            yaw
        );

    // ========================================================
    // LEGS
    // ========================================================

    const Vector3 left_hip =
        world_position(
            feet_position,
            {
                -0.16F,
                0.88F,
                0.0F
            },
            yaw
        );

    const Vector3 right_hip =
        world_position(
            feet_position,
            {
                0.16F,
                0.88F,
                0.0F
            },
            yaw
        );

    const Vector3 left_knee =
        world_position(
            feet_position,
            {
                -0.16F,
                0.47F,
                walk
            },
            yaw
        );

    const Vector3 right_knee =
        world_position(
            feet_position,
            {
                0.16F,
                0.47F,
                opposite_walk
            },
            yaw
        );

    const Vector3 left_ankle =
        world_position(
            feet_position,
            {
                -0.16F,
                0.10F,
                opposite_walk * 0.35F
            },
            yaw
        );

    const Vector3 right_ankle =
        world_position(
            feet_position,
            {
                0.16F,
                0.10F,
                walk * 0.35F
            },
            yaw
        );

    draw_limb(
        left_hip,
        left_knee,
        0.115F,
        pants
    );

    draw_limb(
        left_knee,
        left_ankle,
        0.10F,
        pants
    );

    draw_limb(
        right_hip,
        right_knee,
        0.115F,
        pants
    );

    draw_limb(
        right_knee,
        right_ankle,
        0.10F,
        pants
    );

    // Boots.
    DrawCubeV(
        world_position(
            left_ankle,
            {
                0.0F,
                0.0F,
                0.07F
            },
            yaw
        ),
        {
            0.23F,
            0.17F,
            0.34F
        },
        boots
    );

    DrawCubeV(
        world_position(
            right_ankle,
            {
                0.0F,
                0.0F,
                0.07F
            },
            yaw
        ),
        {
            0.23F,
            0.17F,
            0.34F
        },
        boots
    );

    // ========================================================
    // HIPS + TORSO
    // ========================================================

    DrawCubeV(
        hips,
        {
            0.48F,
            0.30F,
            0.30F
        },
        pants
    );

    DrawCubeV(
        chest,
        {
            0.68F,
            0.72F,
            0.34F
        },
        shirt
    );

    /*
     * Survival vest.
     *
     * Slightly forward from the torso so
     * it reads as equipment rather than
     * simply another torso color.
     */
    DrawCubeV(
        world_position(
            chest,
            {
                0.0F,
                -0.02F,
                0.19F
            },
            yaw
        ),
        {
            0.58F,
            0.53F,
            0.08F
        },
        vest
    );

    // ========================================================
    // ARMS
    // ========================================================

    const Vector3 left_shoulder =
        world_position(
            chest,
            {
                -0.39F,
                0.20F,
                0.0F
            },
            yaw
        );

    const Vector3 right_shoulder =
        world_position(
            chest,
            {
                0.39F,
                0.20F,
                0.0F
            },
            yaw
        );

    const Vector3 left_elbow =
        world_position(
            chest,
            {
                -0.43F,
                -0.15F,
                opposite_walk * 0.85F
            },
            yaw
        );

    const Vector3 right_elbow =
        world_position(
            chest,
            {
                0.43F,
                -0.15F,
                walk * 0.85F
            },
            yaw
        );

    const Vector3 left_hand =
        world_position(
            chest,
            {
                -0.40F,
                -0.48F,
                opposite_walk
            },
            yaw
        );

    const Vector3 right_hand =
        world_position(
            chest,
            {
                0.40F,
                -0.48F,
                walk
            },
            yaw
        );

    draw_limb(
        left_shoulder,
        left_elbow,
        0.09F,
        shirt
    );

    draw_limb(
        left_elbow,
        left_hand,
        0.075F,
        skin
    );

    draw_limb(
        right_shoulder,
        right_elbow,
        0.09F,
        shirt
    );

    draw_limb(
        right_elbow,
        right_hand,
        0.075F,
        skin
    );

    DrawSphere(
        left_hand,
        0.09F,
        skin
    );

    DrawSphere(
        right_hand,
        0.09F,
        skin
    );

    // ========================================================
    // HEAD
    // ========================================================

    DrawCylinder(
        neck,
        0.09F,
        0.10F,
        0.16F,
        8,
        skin
    );

    DrawSphere(
        head,
        0.205F,
        skin
    );

    /*
     * Hair cap.
     */
    DrawSphere(
        world_position(
            head,
            {
                0.0F,
                0.08F,
                -0.015F
            },
            yaw
        ),
        0.19F,
        hair
    );

    /*
     * Face patch drawn slightly forward.
     * This makes facing direction immediately
     * readable during movement testing.
     */
    DrawSphere(
        world_position(
            head,
            {
                0.0F,
                -0.025F,
                0.145F
            },
            yaw
        ),
        0.105F,
        skin
    );
}

} // namespace outland::player
