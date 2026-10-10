#pragma once
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::engine {
// Conservative side-plane culling: no far clip, so gameplay visibility stays unchanged.
class ViewFrustum {
public:
    ViewFrustum(const Camera3D& camera, float aspect) : origin_(camera.position) {
        forward_=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
        right_=Vector3Normalize(Vector3CrossProduct(forward_,camera.up));
        up_=Vector3Normalize(Vector3CrossProduct(right_,forward_));
        valid_=camera.projection==CAMERA_PERSPECTIVE && Vector3Length(right_)>.5F;
        vertical_=std::tan(std::clamp(camera.fovy,1.0F,179.0F)*DEG2RAD*.5F);
        horizontal_=vertical_*std::max(.01F,aspect);
        horizontal_padding_=std::sqrt(1+horizontal_*horizontal_);
        vertical_padding_=std::sqrt(1+vertical_*vertical_);
    }
    bool sphere(Vector3 center,float radius) const {
        if(!valid_) return true;
        radius=std::max(0.0F,radius);
        const auto p=Vector3Subtract(center,origin_);
        const float z=Vector3DotProduct(p,forward_);
        if(z < -radius) return false;
        return std::abs(Vector3DotProduct(p,right_)) <= z*horizontal_+radius*horizontal_padding_ &&
            std::abs(Vector3DotProduct(p,up_)) <= z*vertical_+radius*vertical_padding_;
    }
private:
    Vector3 origin_{},forward_{},right_{},up_{};
    float vertical_{1},horizontal_{1},vertical_padding_{1},horizontal_padding_{1};
    bool valid_{false};
};
}
