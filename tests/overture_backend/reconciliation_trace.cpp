// Differential regression harness: compile with the checkpoint backend and
// with the working backend using the same compiler/options, then compare stdout.
// Covers 512 frames and 944 collision-adapter calls. No HPL/VR runtime needed.
#include "overture_backend.hpp"
#include <bit>
#include <cstdint>
#include <iostream>
namespace ov = penumbra_vr::backends::overture;
namespace rt = penumbra_vr::runtime;
struct Body : ov::OvertureBodyAdapter {
    std::array<float,3> p{0,0.825F,0};
    int tick=0, calls=0;
    std::array<float,3> BodyPosition() const noexcept override { return p; }
    float FeetHeight() const noexcept override { return p[1]-0.825F; }
    std::array<float,3> MoveBodyBy(const std::array<float,3>& d,float,ov::BodyMoveKind kind) noexcept override {
        ++calls;
        float x=1,z=1;
        if (kind==ov::BodyMoveKind::room_scale_static_only) {
            if(tick%4==1) x=z=0;
            if(tick%4==2) x=z=0.4F;
            if(tick%4==3) x=0;
        }
        p[0]+=d[0]*x; p[2]+=d[2]*z;
        return p;
    }
    void StartJump() noexcept override {}
    void SetJumpHeld(bool) noexcept override {}
};
int main() {
    ov::OvertureBackend backend; Body body; std::string error;
    rt::VrMatrix34 pose{{1,0,0,0,0,1,0,1.7F,0,0,1,0}};
    if(!backend.Initialize(pose,body,error)) return 1;
    std::uint64_t hash=14695981039346656037ULL;
    auto add=[&](float f) { hash ^= std::bit_cast<std::uint32_t>(f); hash *= 1099511628211ULL; };
    for(int n=0;n<512;++n) {
        body.tick=n;
        if(n%61==0) body.p[0]+=1.0F;
        body.p[1] = 0.825F + static_cast<float>(n%7)*0.01F;
        pose.values[3]+= (n%5==0 ? 0 : 0.013F);
        pose.values[11]+= (n%3==0 ? -0.019F : 0.007F);
        pose.values[7]=n%80<40 ? 1.7F : 1.0F;
        auto settings=backend.settings();
        settings.play_mode=n%80<40 ? rt::VrPlayMode::standing : rt::VrPlayMode::seated;
        backend.SetSettings(settings);
        backend.tracking_space().SetWorldYaw(static_cast<float>(n%17)*0.1F);
        ov::OvertureInputFrame input;
        input.delta_seconds=0.016F; input.gameplay_active=true;
        input.input.move={true,0.3F,-0.7F};
        input.input.sprint.pressed=n%2==0;
        input.input.recenter.just_pressed=n%19==0;
        ov::OverturePlayerFrame frame{pose,0.016F,n%13!=0,n%9==0};
        ov::OvertureFrameResult result;
        if(!backend.HandleInput(input,body,error) || !backend.UpdatePlayer(frame,body,result,error)) {std::cerr<<error;return 2;}
        for(float f:result.head_anchor) add(f);
        for(float f:result.body_position) add(f);
        add(result.requested_room_scale_distance); add(result.rejected_room_scale_distance);
        add(result.locomotion_distance); add(result.world_yaw_radians); add(result.seated_offset);
        add(result.head_anchor_rebased ? 1.0F : 0.0F);
    }
    std::cout<<std::hex<<hash<<" calls="<<std::dec<<body.calls<<"\n";
}
