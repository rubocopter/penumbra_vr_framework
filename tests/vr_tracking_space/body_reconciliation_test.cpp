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
    auto s = shadow.Observe(Pose(),0,1,Motion(body,body),anchor,0.016F);
    CHECK(s.valid && s.reset && s.plan.physical_request == V{});
    s = shadow.Observe(Pose(0.1F),0,1,Motion(body,body),anchor,0.016F);
    CHECK(s.valid && !s.reset && Near(s.plan.physical_request[0],0.05F));
    CHECK(!s.physical_observation_available && Near(s.predicted_anchor[0],0.1F));
    // Uninjected plan stays unknown, never fabricated as total rejection.
    s = shadow.Observe(Pose(0.1F),0,1,Motion(body,body),anchor,0.016F);
    CHECK(s.plan.physical_request == V{} && Near(s.predicted_anchor[0],0.1F));
    // Replacement at the same position still resets tracking history.
    s = shadow.Observe(Pose(0.2F),0,2,Motion(body,body),anchor,0.016F);
    CHECK(s.reset && s.physical_delta == V{} && s.plan.physical_request == V{});
    s = shadow.Observe(Pose(0.22F),0,2,Motion(body,body),anchor,0.016F);
    CHECK(!s.reset && Near(s.physical_delta[0],0.02F));
    // A load at a distant world coordinate cannot become fake accepted motion.
    s = shadow.Observe(Pose(0.3F),0,3,Motion({100,2,100},{100,2,100}),{100,1.175F,100},0.016F);
    CHECK(s.reset && s.plan.physical_request == V{} && s.predicted_anchor[0] == 100);
    // Same-body teleport / huge native interval / huge tracking discontinuity.
    s = shadow.Observe(Pose(0.3F),0,3,Motion(body,body),anchor,0.016F);
    CHECK(s.reset);
    s = shadow.Observe(Pose(0.3F),0,3,Motion(body,{5,0.825F,0}),{5,0,0},0.016F);
    CHECK(s.reset && s.plan.physical_request == V{});
    s = shadow.Observe(Pose(10),0,3,Motion({5,0.825F,0},{5,0.825F,0}),{5,0,0},0.016F);
    CHECK(s.reset && s.plan.physical_request == V{});
    // Native horizontal motion and jump are observed; no vertical request/correction.
    shadow.Reset();
    s = shadow.Observe(Pose(),0,1,Motion(body,body),anchor,0.016F);
    s = shadow.Observe(Pose(),0,1,Motion(body,{0.05F,0.925F,0}),{0.05F,0.1F,0},0.016F);
    CHECK(s.valid && !s.reset && Near(s.predicted_anchor[0],0.05F));
    CHECK(s.plan.physical_request == V{} && s.native_anchor_correction[1] == 0);
    CHECK(Near(s.predicted_anchor[1],0.1F) && Near(s.native_motion.accepted_displacement[1],0.1F));
    // Black Plague's one owned tick contains the previously accepted physical
    // request and native locomotion. Excluding the physical part must preserve
    // the real body position while carrying the anchor only with locomotion.
    shadow.Reset();
    s = shadow.Observe(Pose(),0,1,Motion(body,body),anchor,0.016F);
    s = shadow.Observe(Pose(0.05F),0,1,Motion(body,body),anchor,0.016F);
    auto physical = Motion(body,{0.05F,0.825F,0});
    CHECK(shadow.ApplyPhysicalReconciliation(
        rt::ReconcilePhysicalBodyMotion(s.plan,physical)));
    s = shadow.Observe(Pose(0.05F),0,1,Motion(body,{0.05F,0.825F,0}),
        {0.05F,0,0},0.016F,physical.accepted_displacement);
    CHECK(Near(s.native_motion.body_after[0],0.05F));
    CHECK(Near(s.locomotion_carry_displacement[0],0.0F));
    CHECK(Near(s.predicted_anchor[0],0.05F));
    // Same-direction native locomotion carries only its own contribution.
    shadow.Reset();
    s = shadow.Observe(Pose(),0,1,Motion(body,body),anchor,0.016F);
    s = shadow.Observe(Pose(0.05F),0,1,Motion(body,body),anchor,0.016F);
    physical = Motion({0.02F,0.825F,0},{0.07F,0.825F,0});
    CHECK(shadow.ApplyPhysicalReconciliation(
        rt::ReconcilePhysicalBodyMotion(s.plan,physical)));
    s = shadow.Observe(Pose(0.05F),0,1,Motion(body,{0.07F,0.825F,0}),
        {0.07F,0,0},0.016F,physical.accepted_displacement);
    CHECK(Near(s.native_motion.body_after[0],0.07F));
    CHECK(Near(s.locomotion_carry_displacement[0],0.02F));
    CHECK(Near(s.predicted_anchor[0],0.07F));
    // Opposing native locomotion likewise carries only its own contribution.
    shadow.Reset();
    s = shadow.Observe(Pose(),0,1,Motion(body,body),anchor,0.016F);
    s = shadow.Observe(Pose(0.05F),0,1,Motion(body,body),anchor,0.016F);
    physical = Motion({-0.02F,0.825F,0},{0.03F,0.825F,0});
    CHECK(shadow.ApplyPhysicalReconciliation(
        rt::ReconcilePhysicalBodyMotion(s.plan,physical)));
    s = shadow.Observe(Pose(0.05F),0,1,Motion(body,{0.03F,0.825F,0}),
        {0.03F,0,0},0.016F,physical.accepted_displacement);
    CHECK(Near(s.locomotion_carry_displacement[0],-0.02F));
    CHECK(Near(s.predicted_anchor[0],0.03F));
    // Native crouch changes centre, preserves feet; no generation change needed.
    s = shadow.Observe(Pose(),0,1,Motion({0.05F,0.575F,0},{0.05F,0.575F,0}),{0.05F,0.1F,0},0.016F);
    CHECK(s.valid && !s.reset && Near(s.predicted_anchor[1],0.1F));
    // Yaw transforms only the new physical delta, not anchor/height.
    shadow.Reset();
    s = shadow.Observe(Pose(),1.570796327F,1,Motion(body,body),anchor,0.016F);
    s = shadow.Observe(Pose(0.02F),1.570796327F,1,Motion(body,body),anchor,0.016F);
    CHECK(Near(s.physical_delta[0],0) && Near(s.physical_delta[2],-0.02F));
    for (float bad_dt : {0.0F,-1.0F,0.251F,nan,inf}) {
        CHECK(!shadow.Observe(Pose(),0,1,Motion(body,body),anchor,bad_dt).valid);
        CHECK(shadow.Observe(Pose(),0,1,Motion(body,body),anchor,0.016F).reset);
    }
    auto bad_pose = Pose(); bad_pose.values[0] = 2;
    CHECK(!shadow.Observe(bad_pose,0,1,Motion(body,body),anchor,0.016F).valid);
    bad_pose = Pose(nan);
    CHECK(!shadow.Observe(bad_pose,0,1,Motion(body,body),anchor,0.016F).valid);
    CHECK(!shadow.Observe(Pose(),inf,1,Motion(body,body),anchor,0.016F).valid);
    CHECK(!shadow.Observe(Pose(),0,0,Motion(body,body),anchor,0.016F).valid);
    CHECK(!shadow.Observe(Pose(),0,1,Motion(body,body),{0,nan,0},0.016F).valid);
    auto bad_motion = Motion(body,body); bad_motion.body_after[0] = inf;
    CHECK(!shadow.Observe(Pose(),0,1,bad_motion,anchor,0.016F).valid);
    // Tracking loss / missing observation explicitly resets at host boundary.
    shadow.Reset();
    CHECK(shadow.Observe(Pose(20),0,1,Motion(body,body),anchor,0.016F).reset);
    std::cout << "Shared reconciliation and BP shadow cases passed\n";
}
