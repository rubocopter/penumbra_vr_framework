#pragma once
// Included inside articulated_grab_fixture. Asset coordinates are from the
// allowlisted level09_rope/contact.dae (manifest articulatedGrab), not equal links.
// HPL Mesh::CreateJointsAndBodies places each body at its collider centre.
inline bool AnchoredRopeCases(bool cable=false) {
    PhysicsApi api;
    if(!api.Complete()) return false;
    physics=&api;
    const std::array<float,7> centres=cable
        ? std::array<float,7>{-2.248817F,-1.678754F,-0.648391F,0.381579F,1.420127F,2.463911F,3.031632F}
        : std::array<float,7>{-1.972998F,-1.235506F,-0.343556F,0.626642F,1.562332F,2.310932F,2.843831F};
    const std::array<float,7> centres_y=cable
        ? std::array<float,7>{0.012821F,0.022159F,0.022159F,0.022159F,0.022159F,0.022159F,0.012821F}
        : std::array<float,7>{0.023167F,0.023167F,0.023167F,0.023167F,0.023167F,0.023167F,0.023167F};
    const std::array<float,7> centres_z=cable
        ? std::array<float,7>{0,-0.007876F,-0.007875F,-0.007875F,-0.007875F,-0.007876F,0}
        : std::array<float,7>{-0.006663F,-0.006663F,-0.006663F,-0.007580F,-0.007580F,-0.008667F,-0.008667F};
    const std::array<float,7> lengths=cable
        ? std::array<float,7>{0.100210F,1.027260F,1.027260F,1.027260F,1.027260F,1.027260F,0.101704F}
        : std::array<float,7>{0.569679F,0.873116F,0.873123F,1.005468F,0.805265F,0.621301F,0.387756F};
    const std::array<float,6> pivots=cable
        ? std::array<float,6>{-2.195186F,-1.165085F,-0.138195F,0.898276F,1.942409F,2.978879F}
        : std::array<float,6>{-1.686078F,-0.798718F,0.094948F,1.136177F,1.985231F,2.641060F};
    bool ok=true;
    for(int hz:{60,90}) for(int fixed:{0,6}) for(int index:{0,3,6}) {
        if(index==fixed) continue;
        Configure(7,true);
        use_normal_mass=true; // level09_rope.ent UseNormalMass=true.
        for(int i=0;i<7;++i) {
            auto pose=runtime::IdentityMatrix();
            pose.values[0]=pose.values[5]=0;
            pose.values[1]=fixed==0 ? 1.0F : -1.0F;
            pose.values[4]=-pose.values[1];
            pose.values[3]=(centres_y[i]-centres_y[index])*pose.values[1];
            pose.values[7]=(centres[i]-centres[index])*pose.values[4];
            pose.values[11]=centres_z[i]-centres_z[index];
            Put(parts[i],0x34,pose);
            Put(parts[i],0x434,i==fixed ? 0.0F : cable && (i==0 || i==6) ? 1.0F : 0.3F);
            Put(parts[i],0x42C,6.0F); Put(parts[i],0x430,6.0F);
        }
        const Vec local_contact{lengths[index]*0.45F,cable ? (index==0 || index==6 ? 0.0878585F : 0.038663F) : 0.036617F,0};
        const auto initial_pose=ReadNative<Matrix>(parts[index],0x34);
        const auto initial_contact=TransformPoint(initial_pose,local_contact);
        g_vr_selection_contact={parts[index],initial_contact,GetTickCount64()};
        if(!Acquire(index)) { physics=nullptr; return false; }
        GrabComponent acquired{};
        const auto anchor_pose=ReadNative<Matrix>(parts[fixed],0x34);
        ok=ok && InspectGrabAssembly(parts[index],true,nullptr,&acquired) &&
            acquired.count==6 && acquired.anchored &&
            !ReadNative<bool>(parts[index],0x428) &&
            ReadNative<float>(parts[index],0x434)==(cable && (index==0 || index==6) ? 1.0F : 0.3F);
        auto* scene=api.NewtonCreate(nullptr,nullptr);
        // requiem_materials.cfg: Silent/Default .4 friction/.2 restitution;
        // Dyn_Generic_Soft .45/.4. Their Average combination owns floor contact.
        const int soft=api.NewtonMaterialCreateGroupID(scene);
        api.NewtonMaterialSetDefaultFriction(scene,0,0,0.4F,0.4F);
        api.NewtonMaterialSetDefaultElasticity(scene,0,0,0.2F);
        api.NewtonMaterialSetDefaultFriction(scene,soft,0,0.425F,0.425F);
        api.NewtonMaterialSetDefaultElasticity(scene,soft,0,0.3F);
        api.NewtonMaterialSetDefaultFriction(scene,soft,soft,0.45F,0.45F);
        api.NewtonMaterialSetDefaultElasticity(scene,soft,soft,0.4F);
        const int metal=api.NewtonMaterialCreateGroupID(scene);
        api.NewtonMaterialSetDefaultFriction(scene,metal,0,0.35F,0.35F);
        api.NewtonMaterialSetDefaultElasticity(scene,metal,0,0.175F);
        api.NewtonMaterialSetDefaultFriction(scene,metal,soft,0.375F,0.375F);
        api.NewtonMaterialSetDefaultElasticity(scene,metal,soft,0.275F);
        api.NewtonMaterialSetDefaultFriction(scene,metal,metal,0.3F,0.3F);
        api.NewtonMaterialSetDefaultElasticity(scene,metal,metal,0.15F);
        std::array<NewtonCollision*,7> rope_shapes{};
        for(int i=0;i<7;++i) {
            rope_shapes[i]=cable && (i==0 || i==6)
                ? api.NewtonCreateBox(scene,lengths[i],0.175717F,0.175717F,nullptr)
                : api.NewtonCreateCylinder(scene,cable ? 0.038663F : 0.036617F,lengths[i],nullptr);
            physical[i]=api.NewtonCreateBody(scene,rope_shapes[i]);
            Vec inertia{},com{};
            api.NewtonConvexCollisionCalculateInertialMatrix(rope_shapes[i],inertia.data(),com.data());
            const auto mass=ReadNative<float>(parts[i],0x434);
            api.NewtonBodySetMassMatrix(physical[i],mass,mass*inertia[0],mass*inertia[1],mass*inertia[2]);
            api.NewtonBodySetCentreOfMass(physical[i],com.data());
            Matrix pose{}; const auto native_pose=ReadNative<Matrix>(parts[i],0x34);
            for(int row=0;row<4;++row) for(int col=0;col<4;++col)
                pose.values[col*4+row]=native_pose.values[row*4+col];
            api.NewtonBodySetMatrix(physical[i],pose.values.data());
            const int material=cable ? (i==0 || i==6 ? metal : i%2==1 ? soft : 0)
                : i%2==0 ? soft : 0;
            api.NewtonBodySetMaterialGroupID(physical[i],material);
            api.NewtonBodySetAutoFreeze(physical[i],0);
            api.NewtonBodySetContinuousCollisionMode(physical[i],1);
            api.NewtonBodySetLinearDamping(physical[i],0.4F);
            const Vec damping{0.4F,0.4F,0.4F};
            api.NewtonBodySetAngularDamping(physical[i],damping.data());
            if(i!=fixed) api.NewtonBodySetForceAndTorqueCallback(physical[i],[](const NewtonBody* body) {
                for(int n=0;n<7;++n) if(physical[n]==body) {
                    if(ReadNative<bool>(parts[n],0x428)) {
                        float mass=0,ix=0,iy=0,iz=0;
                        physics->NewtonBodyGetMassMatrix(body,&mass,&ix,&iy,&iz);
                        const Vec gravity{0,-9.81F*mass,0}; physics->NewtonBodySetForce(body,gravity.data());
                    }
                    Vec linear{},angular{};
                    physics->NewtonBodyGetVelocity(body,linear.data());
                    physics->NewtonBodyGetOmega(body,angular.data());
                    for(auto* value:{&linear,&angular}) {
                        const auto speed=std::hypot((*value)[0],(*value)[1],(*value)[2]);
                        if(speed>6) for(auto& axis:*value) axis*=6/speed;
                    }
                    physics->NewtonBodySetVelocity(body,linear.data());
                    physics->NewtonBodySetOmega(body,angular.data());
                }
            });
        }
        std::array<Vec,6> parent_local{},child_local{};
        for(int i=0;i<6;++i) {
            const Vec pivot=TransformPoint(initial_pose,{pivots[i]-centres[index],0.020955F-centres_y[index],-centres_z[index]});
            auto* joint=api.NewtonConstraintCreateBall(scene,pivot.data(),physical[i+1],physical[i]);
            const Vec pin{initial_pose.values[1],0,0};
            api.NewtonBallSetConeLimits(joint,pin.data(),cable ? 0.17453293F : 0.34906585F,0.69813170F);
            api.NewtonJointSetCollisionState(joint,0);
            parent_local[i]={pivots[i]-centres[i],0.020955F-centres_y[i],-centres_z[i]};
            child_local[i]={pivots[i]-centres[i+1],0.020955F-centres_y[i+1],-centres_z[i+1]};
        }
        // Floor contacts prevent a gravity-free floating chain from hiding the
        // defect. Pull inward/up off that floor, then hold a stationary palm.
        auto* floor_shape=api.NewtonCreateBox(scene,16,0.2F,4,nullptr);
        auto* floor=api.NewtonCreateBody(scene,floor_shape);
        auto floor_pose=runtime::IdentityMatrix(); const float bottom=fixed==0 ? centres[index]-centres[6]-lengths[6]*0.5F
            : centres[0]-centres[index]-lengths[0]*0.5F;
        floor_pose.values[13]=bottom-0.1F;
        api.NewtonBodySetMatrix(floor,floor_pose.values.data());
        const auto root_pivot=TransformPoint(initial_pose,
            {pivots[fixed==0 ? 0 : 5]-centres[index],0.020955F-centres_y[index],-centres_z[index]});
        const float radius_x=initial_contact[0]-root_pivot[0];
        const float radius_y=initial_contact[1]-root_pivot[1];
        float error=0,travel=0,joint_error=0,max_cone=0;
        Vec previous{},low{100,100,100},high{-100,-100,-100};
        for(int tick=0;tick<hz*8;++tick) {
            const float ramp=std::clamp((static_cast<float>(tick)/hz-1.0F)/2.0F,0.0F,1.0F);
            // Reachable arcs inside the authored cones: rope 10/20 degrees,
            // cable 5/10 degrees. Initial palm separation remains unchanged.
            const float angle=(cable ? 0.08726646F : 0.17453293F)*ramp;
            const float dx=std::cos(angle)*radius_x-std::sin(angle)*radius_y-radius_x;
            const float dy=std::sin(angle)*radius_x+std::cos(angle)*radius_y-radius_y;
            Tracking(dy,0,dx);
            HookedGrabUpdate(native_state.data(),nullptr,1.0F/hz);
            api.NewtonUpdate(scene,1.0F/hz);
            for(int i=0;i<7;++i) {
                Matrix native{},pose{}; api.NewtonBodyGetMatrix(physical[i],native.values.data());
                for(int row=0;row<4;++row) for(int col=0;col<4;++col)
                    pose.values[row*4+col]=native.values[col*4+row];
                Put(parts[i],0x34,pose);
            }
            for(int i=0;i<6;++i) {
                const auto a=TransformPoint(ReadNative<Matrix>(parts[i],0x34),parent_local[i]);
                const auto b=TransformPoint(ReadNative<Matrix>(parts[i+1],0x34),child_local[i]);
                joint_error=std::max(joint_error,std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]));
                const auto parent=ReadNative<Matrix>(parts[i],0x34);
                const auto child=ReadNative<Matrix>(parts[i+1],0x34);
                const float pin_dot=parent.values[1]*child.values[1]+
                    parent.values[5]*child.values[5]+parent.values[9]*child.values[9];
                max_cone=std::max(max_cone,std::acos(std::clamp(pin_dot,-1.0F,1.0F)));
            }
            const auto pose=ReadNative<Matrix>(parts[index],0x34);
            const Vec here=TransformPoint(pose,local_contact);
            if(tick>=hz*6) {
                for(int axis=0;axis<3;++axis) { low[axis]=std::min(low[axis],here[axis]); high[axis]=std::max(high[axis],here[axis]); }
                error=std::max(error,std::hypot(here[0]-dx-initial_contact[0],here[1]-dy-initial_contact[1],here[2]-initial_contact[2]));
                travel+=std::hypot(here[0]-previous[0],here[1]-previous[1],here[2]-previous[2]);
            }
            previous=here;
        }
        // A script-style non-solid target volume must actually overlap the
        // held collider, rather than merely accepting a servo request.
        // This is a synthetic oil volume, not execution of level09 callbacks.
        const float final_angle=cable ? 0.08726646F : 0.17453293F;
        const Vec goal{initial_contact[0]+std::cos(final_angle)*radius_x-
            std::sin(final_angle)*radius_y-radius_x,
            initial_contact[1]+std::sin(final_angle)*radius_x+
            std::cos(final_angle)*radius_y-radius_y,initial_contact[2]};
        auto* oil_shape=api.NewtonCreateBox(scene,0.12F,0.12F,0.12F,nullptr);
        auto oil_pose=runtime::IdentityMatrix();
        oil_pose.values[12]=goal[0]; oil_pose.values[13]=goal[1]; oil_pose.values[14]=goal[2];
        Matrix selected_pose{}; api.NewtonBodyGetMatrix(physical[index],selected_pose.values.data());
        Vec collision_point{},collision_normal{}; float penetration=0;
        const bool target_contact=api.NewtonCollisionCollide(scene,1,rope_shapes[index],
            selected_pose.values.data(),oil_shape,oil_pose.values.data(),collision_point.data(),
            collision_normal.data(),&penetration)>0;
        const float excursion=std::hypot(high[0]-low[0],high[1]-low[1],high[2]-low[2]);
        const bool passed=exits==0 && writes==0 && error<0.08F && excursion<0.04F &&
            ReadNative<Matrix>(parts[fixed],0x34).values==anchor_pose.values &&
            joint_error<0.04F && max_cone<(cable ? 0.17453293F : 0.34906585F)+0.05F && target_contact;
        std::cerr<<(cable ? "Anchored cable" : "Anchored rope")<<": hz="<<hz<<" fixed="<<fixed<<" selected="<<index
            <<" error="<<error<<" late travel="<<travel<<" excursion="<<excursion<<" pivot error="<<joint_error<<" cone="<<max_cone<<" volume="<<target_contact<<'\n';
        ok=ok && passed;
        std::array<Vec,7> before_linear{},before_angular{};
        for(int i=0;i<7;++i) {
            api.NewtonBodyGetVelocity(physical[i],before_linear[i].data());
            api.NewtonBodyGetOmega(physical[i],before_angular[i].data());
        }
        test_frame.hands[1].grip.velocity={0,3,0};
        HookedGrabLeave(native_state.data(),nullptr,nullptr);
        bool release_ok=ReadNative<bool>(parts[index],0x428) &&
            ReadNative<float>(parts[index],0x42C)==6 && ReadNative<float>(parts[index],0x430)==6;
        for(int i=0;i<7;++i) {
            Vec after_linear{},after_angular{};
            api.NewtonBodyGetVelocity(physical[i],after_linear.data());
            api.NewtonBodyGetOmega(physical[i],after_angular.data());
            release_ok=release_ok && after_linear==before_linear[i] && after_angular==before_angular[i];
        }
        if(!release_ok) std::cerr<<"Anchored release replaced Newton momentum/gravity/caps\n";
        ok=ok && release_ok;
        physical={};
        api.NewtonReleaseCollision(scene,oil_shape);
        for(auto* shape:rope_shapes) api.NewtonReleaseCollision(scene,shape);
        api.NewtonReleaseCollision(scene,floor_shape); api.NewtonDestroy(scene);
    }
    physics=nullptr;
    return ok;
}
