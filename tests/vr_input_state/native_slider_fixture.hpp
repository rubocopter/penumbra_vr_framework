// Native Newton integration fixture for the BP Move consumer. Included inside
// spatial_interaction_test.cpp so acquisition, palm resolution, update and
// release all run through the production adapter, rather than a servo replica.
struct NativeSliderFixture {
    const NewtonWorld* world=NewtonCreate(nullptr,nullptr);
    NewtonCollision* shape=NewtonCreateBox(world,0.1F,0.1F,0.1F,nullptr);
    NewtonBody* child=NewtonCreateBody(world,shape);
    NewtonBody* parent=NewtonCreateBody(world,shape);
    NewtonJoint* slider=nullptr;
    std::array<std::uint8_t,0x300> entity{},joint{};
    std::array<std::uint8_t,0x500> parent_wrapper{};
    void* wrapper=nullptr;
    unsigned min_callbacks=0,max_callbacks=0,no_limit_callbacks=0,sticky_cancellations=0;
    static constexpr float minimum=0.0F,maximum=0.45F;

    NativeSliderFixture(void* body,float mass,float sticky);
    ~NativeSliderFixture() {
        Put(wrapper,0x41C,static_cast<NewtonBody*>(nullptr));
        NewtonReleaseCollision(world,shape);
        NewtonDestroy(world);
    }
    void PublishPose() {
        Matrix native{};
        NewtonBodyGetMatrix(child,native.values.data());
        Matrix hpl{};
        for (std::size_t row=0;row<4;++row)
            for (std::size_t column=0;column<4;++column)
                hpl.values[row*4+column]=native.values[column*4+row];
        Put(wrapper,0x34,hpl);
    }
    void Step() {
        NewtonUpdate(world,1.0F/120.0F);
        PublishPose();
    }
    float Position() const { return NewtonSliderGetJointPosit(slider); }
    static unsigned Limit(const NewtonJoint*,NewtonHingeSliderUpdateDesc*);
};

// Independently mapped from artifacts/black-plague-22000-live.bin, SHA256
// 2881741D3853DBBACF1342A8A84440AC1F36AF30677888050DA35253235A8295:
// constructor 19F5D0 registers callback 19F390. Its sticky branch 19F485..19F547
// reads E8/EC, then zeros omega and velocity (19F51D/19F542). Limits at
// 19F3F7..19F47B use 260/25C first. This transcription keeps those native
// decisions; actual integration and stop acceleration use the shipped DLL
// (SHA256 69E6F1BA9D02E7FEBEFCB6DF9BFC6A5A8D8ED78FA659E86B06CA301BFAC894EE,
// identical to the installed BP Newton.dll). Limit auto-sleep is off here;
// this fixture does not model native audio, scripts or whole-engine scheduling.
// RED before hold-time sticky suppression: desk passes, closed cabinet stays
// at position 0 after 124 native cancellations. GREEN exercises the consumer
// with the same callback and DLL, including restoration on release.
NativeSliderFixture::NativeSliderFixture(void* body,float mass,float sticky)
    : wrapper(body) {
    Put(entity.data(),kEntityActiveOffset,true);
    Put(entity.data(),kEntityTypeOffset,kObjectEntityType);
    Put(parent_wrapper.data(),0,g_image+kPhysicsBodyVtable);
    Put(parent_wrapper.data(),0x434,0.0F);
    Put(joint.data(),0,g_image+kPhysicsJointSliderNewtonVtable);
    Put(joint.data(),0x2C,static_cast<void*>(parent_wrapper.data()));
    Put(joint.data(),0x30,body);
    Put(joint.data(),kJointPinDirectionOffset,Vec{1,0,0});
    Put(joint.data(),kJointPivotPointOffset,Vec{});
    Put(joint.data(),0xE8,sticky);
    Put(joint.data(),0xEC,sticky);
    Put(joint.data(),0x260,minimum);
    Put(joint.data(),0x25C,maximum);
    Put(body,kBodyUserDataOffset,static_cast<void*>(entity.data()));
    Put(body,0x434,mass);
    Put(body,0x41C,child);
    Put(body,0x42C,3.0F);
    Put(body,0x430,3.0F);
    NewtonBodySetMassMatrix(child,mass,mass*0.01F,mass*0.01F,mass*0.01F);
    NewtonBodySetAutoFreeze(child,0);
    NewtonBodySetLinearDamping(child,1.0F); // installed cabinet drawers
    auto parent_pose=runtime::IdentityMatrix();
    parent_pose.values[12]=-1.0F;
    NewtonBodySetMatrix(parent,parent_pose.values.data());
    const Vec origin{},pin{1,0,0};
    slider=NewtonConstraintCreateSlider(world,origin.data(),pin.data(),child,parent);
    NewtonJointSetUserData(slider,this);
    NewtonSliderSetUserCallback(slider,&Limit);
    Put(joint.data(),0x270,slider);
    PublishPose();
}

unsigned NativeSliderFixture::Limit(const NewtonJoint* native,
    NewtonHingeSliderUpdateDesc* desc) {
    auto& fixture=*static_cast<NativeSliderFixture*>(NewtonJointGetUserData(native));
    const float position=NewtonSliderGetJointPosit(native);
    const float low=Read<float>(fixture.joint.data(),0x260);
    const float high=Read<float>(fixture.joint.data(),0x25C);
    if (low==0 && high==0) return 0;
    if (position<low) {
        ++fixture.min_callbacks;
        desc->m_accel=NewtonSliderCalculateStopAccel(native,desc,low);
        desc->m_minFriction=0;
        return 1;
    }
    if (position>high) {
        ++fixture.max_callbacks;
        desc->m_accel=NewtonSliderCalculateStopAccel(native,desc,high);
        desc->m_maxFriction=0;
        return 1;
    }
    const auto* parent_body=Read<void*>(fixture.joint.data(),0x2C);
    const float sticky_min=Read<float>(fixture.joint.data(),0xE8);
    const float sticky_max=Read<float>(fixture.joint.data(),0xEC);
    if ((!parent_body || Read<float>(parent_body,0x434)==0) &&
        ((sticky_max!=0 && std::abs(position-high)<sticky_max) ||
         (sticky_min!=0 && std::abs(position-low)<sticky_min))) {
        const Vec zero{};
        NewtonBodySetOmega(fixture.child,zero.data());
        NewtonBodySetVelocity(fixture.child,zero.data());
        ++fixture.sticky_cancellations;
    }
    ++fixture.no_limit_callbacks;
    return 0;
}

int RunNativeSliderRegression(void* player,void* state,void* body,
    runtime::VrHmdPose& hand) {
    Put(g_image+kPhysicsJointSliderNewtonVtable,kJointTypeVtableSlot,
        g_image+kSliderGetType);
    // Desk is the zero-sticky control; cabinet has the installed 1 cm latches.
    // All sweeps drive the actual BP consumer and native Newton integration.
    for (const float sticky : {0.0F,0.01F}) for (const float mass : {1.0F,3.0F,25.0F}) {
        NativeSliderFixture fixture(body,mass,sticky);
        test_joint=fixture.joint.data();
        test_joints=1;
        Put(state,0x38,Vec{});
        hand.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};
        test_resolved_palm=runtime::IdentityMatrix();
        test_resolved_palm_valid=true;
        test_resolved_palm_generation=1;
        test_palm_resolver_publish_on_service=true;
        test_frame.input.state.interact.pressed=true;
        test_frame.input.state.interact.just_pressed=true;
        g_vr_selection_ready=true;
        g_vr_selection_player=player;
        const int leaves_before=test_move_leaves;
        const int writes_before=test_matrix_writes;
        HookedMoveEnter(state,nullptr,nullptr);
        Put(player,0x2BC,2);
        ServiceSpatialInteraction(player,false);
        if (!g_move_held.load() || g_move_hold.mode!=MoveHold::Mode::slider) return 254;
        const auto step=[&] {
            HookedMoveUpdate(state,nullptr,1.0F/120.0F);
            fixture.Step();
        };
        // Acquisition must preserve the initial contact, including at a latch.
        for (int i=0;i<4;++i) step();
        if (std::abs(fixture.Position())>0.0001F) return 255;
        // Only the arm moves. Raw aim is deliberately farther away and off-axis;
        // the resolved palm remains the manipulation target.
        hand.device_to_absolute.values[3]=0.15F;
        hand.device_to_absolute.values[7]=0.04F;
        test_resolved_palm.values[3]=0.08F;
        for (int i=0;i<120;++i) step();
        if (!g_move_held.load() || std::abs(fixture.Position()-0.08F)>0.005F) {
            std::cerr << "Move slider cannot leave closed endpoint: sticky=" << sticky
                      << " mass=" << mass << " position=" << fixture.Position()
                      << " native cancellations=" << fixture.sticky_cancellations << '\n';
            return 256;
        }
        // Real obstruction: it must stop at the blocker rather than teleport,
        // and resume toward the same stationary hand after the blocker is gone.
        auto* blocker=NewtonCreateBody(fixture.world,fixture.shape);
        auto blocker_pose=runtime::IdentityMatrix();
        blocker_pose.values[12]=0.25F;
        NewtonBodySetMatrix(blocker,blocker_pose.values.data());
        test_resolved_palm.values[3]=0.30F;
        for (int i=0;i<120;++i) step();
        if (fixture.Position()<0.10F || fixture.Position()>0.17F) return 257;
        NewtonDestroyBody(fixture.world,blocker);
        for (int i=0;i<120;++i) step();
        if (std::abs(fixture.Position()-0.30F)>0.005F) return 258;
        // Pull past the authored stop, then reverse out of the max latch.
        test_resolved_palm.values[3]=0.60F;
        for (int i=0;i<120;++i) step();
        // Newton's native stop is compliant under a continuing velocity drive;
        // the unmodified desk control settles about 7 mm beyond this stop.
        if (std::abs(fixture.Position()-0.45F)>0.01F || !fixture.max_callbacks) {
            std::cerr << "native max stop: sticky=" << sticky << " mass=" << mass
                      << " position=" << fixture.Position() << " callbacks="
                      << fixture.max_callbacks << " held=" << g_move_held.load() << '\n';
            return 259;
        }
        test_resolved_palm.values[3]=0.40F;
        for (int i=0;i<120;++i) step();
        if (std::abs(fixture.Position()-0.40F)>0.005F) return 260;
        // Close past min, preserving the native min-limit callback too.
        test_resolved_palm.values[3]=0.10F;
        for (int i=0;i<120;++i) step();
        test_resolved_palm.values[3]=-0.05F;
        for (int i=0;i<120;++i) step();
        if (std::abs(fixture.Position())>0.005F || !fixture.min_callbacks ||
            !fixture.no_limit_callbacks || test_matrix_writes!=writes_before) return 261;
        // A script tightening travel during the hold remains authoritative.
        Put(fixture.joint.data(),0x25C,0.12F);
        test_resolved_palm.values[3]=0.20F;
        for (int i=0;i<120;++i) step();
        if (fixture.Position()<0.115F || fixture.Position()>0.13F) return 265;
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player,false);
        if (g_move_held.load() || test_move_leaves!=leaves_before+1 ||
            Read<float>(body,0x42C)!=3 || Read<float>(body,0x430)!=3 ||
            Read<float>(fixture.joint.data(),0xE8)!=sticky ||
            Read<float>(fixture.joint.data(),0xEC)!=sticky) return 262;
        // Native latching returns on release, including a fresh closing impulse.
        auto closed_pose=runtime::IdentityMatrix();
        closed_pose.values[12]=0.005F;
        NewtonBodySetMatrix(fixture.child,closed_pose.values.data());
        const Vec slow_open{0.01F,0,0};
        NewtonBodySetVelocity(fixture.child,slow_open.data());
        const auto cancellations_before=fixture.sticky_cancellations;
        fixture.Step();
        if (sticky>0 && (fixture.sticky_cancellations<=cancellations_before ||
            std::abs(fixture.Position()-0.005F)>0.0001F)) return 263;
        if (Read<float>(fixture.joint.data(),0x260)!=0 ||
            Read<float>(fixture.joint.data(),0x25C)!=0.12F) return 264;
        if (sticky>0 && mass==3) {
            for (int cancellation=0;cancellation<5;++cancellation) {
                fixture.PublishPose();
                hand.device_to_absolute={{1,0,0,0.005F,0,1,0,0,0,0,1,0}};
                test_resolved_palm=runtime::IdentityMatrix();
                test_resolved_palm.values[3]=0.005F;
                test_frame.input.state.interact.pressed=true;
                test_frame.input.state.interact.just_pressed=true;
                g_vr_selection_ready=true;
                g_vr_selection_player=player;
                HookedMoveEnter(state,nullptr,nullptr);
                Put(player,0x2BC,2);
                ServiceSpatialInteraction(player,false);
                if (!g_move_held.load()) return 266;
                if (cancellation==0) test_frame.focused=false;
                // Invalid dt exercises the update guard; focus/UI exercise
                // the service-owned release without reaching another solve.
                if (cancellation==1) {
                    ServiceSpatialInteraction(player,true);
                } else if (cancellation==2) {
                    HookedMoveUpdate(state,nullptr,-1.0F);
                } else if (cancellation==3) {
                    // Preserve a script's explicit sticky setting on release.
                    Put(fixture.joint.data(),0xE8,0.02F);
                    test_frame.input.state.interact.pressed=false;
                    ServiceSpatialInteraction(player,false);
                } else if (cancellation==4) {
                    // Existing same-player recovery when Leave was missed.
                    Put(player,0x2BC,0);
                    ServiceSpatialInteraction(player,false);
                } else {
                    ServiceSpatialInteraction(player,false);
                }
                if (g_move_held.load() ||
                    Read<float>(fixture.joint.data(),0xE8)!=(cancellation==3 ? 0.02F : sticky) ||
                    Read<float>(fixture.joint.data(),0xEC)!=sticky) return 267;
                Put(fixture.joint.data(),0xE8,sticky);
                test_frame.focused=true;
                test_resolved_palm_valid=true;
            }
            // Rework's discovery can follow a selected parent body through a
            // linked child to its slider. That is not permission to override
            // the other child's latches: acquisition and release must agree.
            std::array<std::uint8_t,0x500> neighbour{};
            std::array<std::uint8_t,0x300> link{};
            Put(neighbour.data(),0,g_image+kPhysicsBodyVtable);
            Put(link.data(),0x2C,body);
            Put(link.data(),0x30,static_cast<void*>(neighbour.data()));
            Put(fixture.joint.data(),0x30,static_cast<void*>(neighbour.data()));
            test_joint_graph={{body,{link.data()}},
                {neighbour.data(),{link.data(),fixture.joint.data()}}};
            test_frame.input.state.interact.pressed=true;
            test_frame.input.state.interact.just_pressed=true;
            g_vr_selection_ready=true;
            g_vr_selection_player=player;
            HookedMoveEnter(state,nullptr,nullptr);
            Put(player,0x2BC,2);
            ServiceSpatialInteraction(player,false);
            if (!g_move_held.load() ||
                Read<float>(fixture.joint.data(),0xE8)!=sticky ||
                Read<float>(fixture.joint.data(),0xEC)!=sticky) return 268;
            test_frame.input.state.interact.pressed=false;
            ServiceSpatialInteraction(player,false);
            if (g_move_held.load() || Read<float>(fixture.joint.data(),0xE8)!=sticky ||
                Read<float>(fixture.joint.data(),0xEC)!=sticky) return 269;
            test_joint_graph.clear();
        }
        test_resolved_palm_valid=false;
        test_palm_resolver_publish_on_service=false;
        Put(body,kBodyUserDataOffset,static_cast<void*>(nullptr));
        Put(body,0x34,runtime::IdentityMatrix());
    }
    test_joint=nullptr;
    test_joints=0;
    Put(body,0x434,2.0F);
    Put(body,0x42C,3.0F);
    Put(body,0x430,4.0F);
    hand.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};
    Put(player,0x2BC,0);
    return 0;
}
