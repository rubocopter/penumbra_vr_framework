#pragma once

#include "vr_grab_pose.hpp"
#include "vr_mechanism_policy.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace penumbra_vr::runtime::vr_articulated_grab_policy {
using Vec = std::array<float,3>;
using Matrix = VrMatrix44;

// BP's bounded contact and wrist drive, consumed by BP and Requiem.
// Native graph discovery, gravity, joint solving and velocity application
// remain backend-owned. No body matrix is written by this policy.
[[nodiscard]] inline Vec TransformPoint(const Matrix& matrix, const Vec& point) noexcept {
    Vec result{};
    for (std::size_t row=0;row<3;++row)
        result[row]=matrix.values[row*4]*point[0]+
            matrix.values[row*4+1]*point[1]+
            matrix.values[row*4+2]*point[2]+matrix.values[row*4+3];
    return result;
}

[[nodiscard]] inline bool ArticulatedContactImpulse(const Matrix& body_pose,
    const Matrix& destination,const Vec& contact,const Vec& tracked_velocity,
    const Vec& linear,const Vec& angular,float dt,Vec& impulse,
    Vec& world_contact) noexcept {
    impulse={};
    world_contact={};
    // A stalled update must not integrate a stale positional error into a
    // large impulse. Short/normal ticks use a per-second acceleration budget.
    if (!std::isfinite(dt) || dt<=0 || dt>0.10F ||
        !vr_mechanism_policy::Finite(contact) || !vr_mechanism_policy::Finite(tracked_velocity) ||
        !vr_mechanism_policy::Finite(linear) || !vr_mechanism_policy::Finite(angular)) return false;
    world_contact=TransformPoint(body_pose,contact);
    const Vec desired_contact=TransformPoint(destination,contact);
    const Vec centre{body_pose.values[3],body_pose.values[7],body_pose.values[11]};
    if (!vr_mechanism_policy::Finite(world_contact) || !vr_mechanism_policy::Finite(desired_contact) ||
        !vr_mechanism_policy::Finite(centre)) return false;
    const Vec radius{world_contact[0]-centre[0],world_contact[1]-centre[1],
        world_contact[2]-centre[2]};
    const Vec tangent=vr_mechanism_policy::Cross(angular,radius);
    const Vec hand_speed=runtime::LimitTrackedVelocity(tracked_velocity,1,3);
    Vec requested{};
    for (int axis=0;axis<3;++axis)
        requested[axis]=hand_speed[axis]+(desired_contact[axis]-world_contact[axis])*9;
    requested=runtime::LimitTrackedVelocity(requested,1,3);
    for (int axis=0;axis<3;++axis)
        impulse[axis]=requested[axis]-linear[axis]-tangent[axis];
    impulse=runtime::LimitTrackedVelocity(impulse,1,
        std::min(2.3F,dt*145.0F));
    return vr_mechanism_policy::Finite(impulse);
}
[[nodiscard]] inline bool ArticulatedAngularDrive(const Matrix& body_pose,
    const Matrix& destination,const Vec& hand_angular,const Vec& current,
    float dt,Vec& corrected) noexcept {
    corrected={};
    if (!std::isfinite(dt) || dt<=0 || dt>0.10F ||
        !vr_mechanism_policy::Finite(hand_angular) || !vr_mechanism_policy::Finite(current)) return false;
    Vec angular_error{};
    for (int axis=0;axis<3;++axis) {
        const Vec from{body_pose.values[axis],body_pose.values[axis+4],
            body_pose.values[axis+8]};
        const Vec to{destination.values[axis],destination.values[axis+4],
            destination.values[axis+8]};
        if (!vr_mechanism_policy::Finite(from) || !vr_mechanism_policy::Finite(to)) return false;
        const Vec difference=vr_mechanism_policy::Cross(from,to);
        for (int component=0;component<3;++component)
            angular_error[component]+=difference[component]*0.5F;
    }
    // The cross products above encode axis*sin(angle), which vanishes at a
    // half-turn. Recover the shortest axis-angle error, including that case.
    std::array<float,9> relative{};
    for (int row=0;row<3;++row)
        for (int col=0;col<3;++col)
            for (int axis=0;axis<3;++axis)
                relative[row*3+col]+=destination.values[row*4+axis]*
                    body_pose.values[col*4+axis];
    const float cosine=std::clamp(
        (relative[0]+relative[4]+relative[8]-1.0F)*0.5F,-1.0F,1.0F);
    const float angle=std::acos(cosine);
    const float sine=std::hypot(angular_error[0],angular_error[1],angular_error[2]);
    if (cosine<0 && sine<0.001F) {
        // At pi choose the best-conditioned diagonal. The symmetric terms
        // recover the other components; the skew picks the side near pi.
        int largest=0;
        for (int axis=1;axis<3;++axis)
            if (relative[axis*3+axis]>relative[largest*3+largest]) largest=axis;
        Vec rotation_axis{};
        rotation_axis[largest]=std::sqrt(std::max(0.0F,
            (relative[largest*3+largest]+1.0F)*0.5F));
        if (rotation_axis[largest]<0.001F) return false;
        for (int axis=0;axis<3;++axis)
            if (axis!=largest)
                rotation_axis[axis]=(relative[largest*3+axis]+
                    relative[axis*3+largest])/(4.0F*rotation_axis[largest]);
        const float direction=vr_mechanism_policy::Dot(rotation_axis,angular_error)<0 ? -1.0F : 1.0F;
        for (int axis=0;axis<3;++axis)
            angular_error[axis]=rotation_axis[axis]*angle*direction;
    } else if (sine>1.0e-6F) {
        for (float& component:angular_error) component*=angle/sine;
    }
    Vec requested{};
    for (int axis=0;axis<3;++axis)
        requested[axis]=hand_angular[axis]*0.5F+angular_error[axis]*4;
    requested=runtime::LimitTrackedVelocity(requested,1,3);
    Vec step{};
    for (int axis=0;axis<3;++axis)
        step[axis]=requested[axis]-current[axis];
    step=runtime::LimitTrackedVelocity(step,1,std::min(0.20F,dt*12.5F));
    for (int axis=0;axis<3;++axis)
        corrected[axis]=current[axis]+step[axis];
    return vr_mechanism_policy::Finite(corrected);
}
} // namespace penumbra_vr::runtime::vr_articulated_grab_policy
