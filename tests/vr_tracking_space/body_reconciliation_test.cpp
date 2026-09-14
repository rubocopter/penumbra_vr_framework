#include "body_reconciliation_shadow.hpp"
#include "vr_locomotion.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace rt = penumbra_vr::runtime;
namespace bp = penumbra_vr::backends::black_plague;
using V = std::array<float, 3>;
#define CHECK(x) do { if (!(x)) { std::cerr << "Failed line " << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
bool Near(float a, float b) { return std::abs(a-b) < 0.00001F; }
rt::VrMatrix34 Pose(float x = 0, float y = 1.7F, float z = 0) {
    return {{1,0,0,x, 0,1,0,y, 0,0,1,z}};
}
rt::VrTrackingSampleIdentity Identity(
    std::uint64_t sequence,
    std::uint64_t pose_epoch = 1,
    std::uint64_t yaw_epoch = 1) {
    return {sequence, sequence * 10, pose_epoch, yaw_epoch};
}
rt::VrAcceptedBodyMotion Motion(V a, V b) {
    rt::VrAcceptedBodyMotion motion;
    CHECK(rt::ObserveAcceptedBodyMotion(a,b,motion));
    return motion;
}
int main() {
    const V body{0,0.825F,0}, anchor{0,0,0};
    auto plan = rt::PlanBodyReconciliation(anchor,body,{});
    CHECK(plan.valid && plan.physical_request == V{});
    plan = rt::PlanBodyReconciliation(anchor,body,{0.02F,10,0});
    CHECK(Near(plan.physical_request[0],0.02F) && plan.physical_request[1] == 0);
    plan = rt::PlanBodyReconciliation(anchor,body,{0.10F,0,0});
    CHECK(Near(plan.physical_request[0],0.05F));
    auto full = rt::ReconcilePhysicalBodyMotion(plan,Motion(body,{0.05F,0.825F,0}));
    CHECK(full.valid && Near(full.rejected_distance,0) && Near(full.head_anchor[0],0.1F));
    auto blocked = rt::ReconcilePhysicalBodyMotion(plan,Motion(body,body));
    CHECK(blocked.valid && Near(blocked.head_anchor[0],0.05F));
    auto partial = rt::ReconcilePhysicalBodyMotion(plan,Motion(body,{0.02F,0.825F,0}));
    CHECK(Near(partial.rejected_distance,0.03F) && Near(partial.head_anchor[0],0.07F));
    // Lateral sliding must not be mistaken for distance along the request.
    auto slide = rt::ReconcilePhysicalBodyMotion(plan,Motion(body,{0.02F,1.3F,0.04F}));
    CHECK(Near(slide.head_anchor[0],0.07F) && slide.anchor_correction[2] == 0);
    CHECK(slide.anchor_correction[1] == 0 && slide.head_anchor[1] == anchor[1]);
    auto epsilon = rt::ReconcilePhysicalBodyMotion(plan,Motion(body,{0.049F,0.825F,0}));
    CHECK(epsilon.anchor_correction == V{});
    CHECK(rt::PlanBodyReconciliation({0.05F,0,0},body,{}).physical_request == V{});
    auto rebase = rt::PlanBodyReconciliation({1,0,0},body,{0.02F,0,0});
    CHECK(rebase.rebased && Near(rebase.head_anchor[0],0.02F) && rebase.head_anchor[1] == body[1]);
    CHECK(!rt::PlanBodyReconciliation({0.8F,0,0},body,{}).rebased);
    auto carry = rt::CarryHeadAnchorWithLocomotion(anchor,Motion(body,{0.1F,0.825F,0}));
    CHECK(Near(carry[0],0.1F));
    // Body approaches an ahead-of-body head anchor: carrying would increase separation.
    CHECK(Near(rt::CarryHeadAnchorWithLocomotion({0.2F,0,0},Motion(body,{0.1F,0.825F,0}))[0],0.2F));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (float bad : {nan,inf,-inf}) {
        CHECK(!rt::PlanBodyReconciliation(anchor,body,{bad,0,0}).valid);
        CHECK(!rt::PlanBodyReconciliation(anchor,{bad,0,0},{}).valid);
        CHECK(!rt::PlanBodyReconciliation({bad,0,0},body,{}).valid);
        auto invalid_motion = Motion(body,body);
        invalid_motion.accepted_displacement[0] = bad;
        CHECK(!rt::ReconcilePhysicalBodyMotion(plan,invalid_motion).valid);
    }
    // Even finite but malformed external plans must not emit NaN diagnostics.
    auto oversized = plan;
    oversized.physical_request = {1.0e19F, 0, 1.0e19F};
    CHECK(!rt::ReconcilePhysicalBodyMotion(oversized,
        Motion(body,{1.0e20F,0.825F,-1.0e20F})).valid);
    bp::BodyReconciliationShadow shadow;
    auto tick = shadow.PrepareTick(Pose(),0,1,body,0.016F,Identity(1));
    CHECK(tick.valid && tick.reset && tick.reconciliation.physical_request == V{});
    CHECK(tick.tracking_identity.sequence == 1 &&
        tick.tracking_identity.pose_epoch == 1 &&
        Near(tick.tracking_pose.values[3],0));
    auto s = shadow.CompleteTick(tick,Motion(body,body),anchor);
    CHECK(s.valid && s.reset && Near(s.predicted_anchor[0],0));

    // Planning happens against B0 before the native tick. A free request is
    // reconciled in that same transaction and a repeated pose is integrated once.
    tick = shadow.PrepareTick(Pose(0.05F),0,1,body,0.016F,Identity(2));
    CHECK(tick.valid && !tick.reset && Near(tick.reconciliation.physical_request[0],0.05F));
    CHECK(tick.tracking_identity.sequence == 2 &&
        Near(tick.tracking_pose.values[3],0.05F));
    auto physical = Motion(body,{0.05F,0.825F,0});
    s = shadow.CompleteTick(tick,physical,{0.05F,0,0},true,physical);
    CHECK(s.valid && s.physical_observation_available);
    CHECK(Near(s.predicted_anchor[0],0.05F));
    tick = shadow.PrepareTick(Pose(0.05F),0,1,{0.05F,0.825F,0},0.016F);
    CHECK(tick.valid && tick.physical_delta == V{} &&
        tick.reconciliation.physical_request == V{});
    s = shadow.CompleteTick(tick,
        Motion({0.05F,0.825F,0},{0.05F,0.825F,0}),{0.05F,0,0});
    CHECK(s.valid && Near(s.predicted_anchor[0],0.05F));

    // A blocked request corrects the anchor in the same tick. The next
    // stationary tick must not enqueue an opposite/residual request.
    shadow.Reset();
    tick = shadow.PrepareTick(Pose(),0,1,body,0.016F);
    s = shadow.CompleteTick(tick,Motion(body,body),anchor);
    tick = shadow.PrepareTick(Pose(0.05F),0,1,body,0.016F);
    CHECK(Near(tick.reconciliation.physical_request[0],0.05F));
    physical = Motion(body,body);
    s = shadow.CompleteTick(tick,Motion(body,body),anchor,true,physical);
    CHECK(s.valid && Near(s.predicted_anchor[0],0));
    tick = shadow.PrepareTick(Pose(0.05F),0,1,body,0.016F);
    CHECK(tick.physical_delta == V{} && tick.reconciliation.physical_request == V{});
    s = shadow.CompleteTick(tick,Motion(body,body),anchor);
    CHECK(Near(s.predicted_anchor[0],0));

    // Slide resolution uses the accepted projection for physical reconciliation
    // while preserving the observed lateral total as derived non-physical carry.
    shadow.Reset();
    tick = shadow.PrepareTick(Pose(),0,1,body,0.016F);
    s = shadow.CompleteTick(tick,Motion(body,body),anchor);
    tick = shadow.PrepareTick(Pose(0.05F),0,1,body,0.016F);
    physical = Motion(body,{0.02F,0.825F,0.03F});
    s = shadow.CompleteTick(tick,physical,{0.02F,0,0.03F},true,physical);
    CHECK(s.valid && Near(s.physical_reconciliation.rejected_distance,0.03F));

    // Replacement at the same position and recenter-like generation changes
    // reset history instead of interpreting the discontinuity as motion.
    tick = shadow.PrepareTick(Pose(0.2F),0,2,physical.body_after,0.016F);
    CHECK(tick.valid && tick.reset && tick.physical_delta == V{} &&
        tick.reconciliation.physical_request == V{});
    s = shadow.CompleteTick(tick,
        Motion(physical.body_after,physical.body_after),{0.02F,0,0.03F});
    CHECK(s.valid && s.reset);

    // Yaw rotates only the newly observed HMD delta.
    shadow.Reset();
    tick = shadow.PrepareTick(Pose(),1.570796327F,1,body,0.016F);
    s = shadow.CompleteTick(tick,Motion(body,body),anchor);
    tick = shadow.PrepareTick(Pose(0.02F),1.570796327F,1,body,0.016F);
    CHECK(Near(tick.physical_delta[0],0) && Near(tick.physical_delta[2],-0.02F));
    s = shadow.CompleteTick(tick,Motion(body,body),anchor);

    // Uniform free ramps at a 60 Hz physics tick never request the opposite
    // direction, including when presentation samples arrive at 72/90/120 Hz.
    for (const int render_hz : {72,90,120}) {
        shadow.Reset();
        V simulated_body = body;
        tick = shadow.PrepareTick(Pose(),0,1,simulated_body,1.0F/60.0F);
        s = shadow.CompleteTick(tick,Motion(simulated_body,simulated_body),anchor);
        float previous_request = 0.0F;
        for (int physics_frame = 1; physics_frame <= 120; ++physics_frame) {
            const float time = static_cast<float>(physics_frame) / 60.0F;
            const float sampled_time = std::floor(time * render_hz) /
                static_cast<float>(render_hz);
            const float head_x = sampled_time * 0.20F;
            tick = shadow.PrepareTick(
                Pose(head_x),0,1,simulated_body,1.0F/60.0F);
            CHECK(tick.valid);
            const float request = tick.reconciliation.physical_request[0];
            CHECK(request >= -0.000001F && request <= 0.050001F);
            if (previous_request > 0.0F) CHECK(request >= -0.000001F);
            previous_request = request;
            V after = simulated_body;
            after[0] += request;
            physical = Motion(simulated_body,after);
            s = shadow.CompleteTick(tick,physical,{after[0],0,after[2]},
                std::abs(request) > 0.0F,physical);
            CHECK(s.valid);
            simulated_body = after;
        }
    }

    // Sub-2 mm stationary tracking jitter may legitimately alternate sign, but
    // each sample is integrated exactly once. If the native solve accepts the
    // tiny motion, returning the HMD to its origin must not leave accumulated
    // body/anchor drift or a stale request for the following tick.
    shadow.Reset();
    V jitter_body = body;
    tick = shadow.PrepareTick(Pose(),0,1,jitter_body,1.0F/60.0F);
    s = shadow.CompleteTick(tick,Motion(jitter_body,jitter_body),anchor);
    for (const float head_x : {0.0010F,-0.0008F,0.0012F,-0.0006F,0.0F}) {
        tick = shadow.PrepareTick(
            Pose(head_x),0,1,jitter_body,1.0F/60.0F);
        CHECK(tick.valid);
        const float request = tick.reconciliation.physical_request[0];
        CHECK(std::abs(request) <= 0.002001F);
        V after = jitter_body;
        after[0] += request;
        physical = Motion(jitter_body,after);
        s = shadow.CompleteTick(tick,physical,{after[0],0,0},
            std::abs(request) > 0.0F,physical);
        CHECK(s.valid);
        jitter_body = after;
    }
    CHECK(Near(jitter_body[0],0.0F));
    CHECK(Near(s.predicted_anchor[0],0.0F));
    tick = shadow.PrepareTick(Pose(),0,1,jitter_body,1.0F/60.0F);
    CHECK(tick.valid && Near(tick.reconciliation.physical_request[0],0.0F));

    // Malformed samples and invalid timing reset the transaction rather than
    // leaving a pending plan that a later tick could accidentally consume.
    for (float bad_dt : {0.0F,-1.0F,0.251F,nan,inf}) {
        shadow.Reset();
        CHECK(!shadow.PrepareTick(Pose(),0,1,body,bad_dt).valid);
        tick = shadow.PrepareTick(Pose(),0,1,body,0.016F);
        CHECK(tick.valid && tick.reset);
        CHECK(shadow.CompleteTick(tick,Motion(body,body),anchor).valid);
    }
    auto bad_pose = Pose(); bad_pose.values[0] = 2;
    CHECK(!shadow.PrepareTick(bad_pose,0,1,body,0.016F).valid);
    bad_pose = Pose(nan);
    CHECK(!shadow.PrepareTick(bad_pose,0,1,body,0.016F).valid);
    CHECK(!shadow.PrepareTick(Pose(),inf,1,body,0.016F).valid);
    CHECK(!shadow.PrepareTick(Pose(),0,0,body,0.016F).valid);
    std::cout << "Shared reconciliation and BP shadow cases passed\n";
}
