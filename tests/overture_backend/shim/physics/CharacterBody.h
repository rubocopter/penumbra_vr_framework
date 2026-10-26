#pragma once
#include "math/Math.h"
namespace hpl {
class iCharacterBody {
public:
    cVector3f position{0.0f}, vr_velocity{0.0f};
    bool vr_stepstaticonly=false, blocked=false;
    int updates=0;
    cVector3f GetPosition() const { return position; }
    cVector3f GetFeetPosition() const { return {position.x,0.0f,position.z}; }
    void Update(float) { ++updates; if(!blocked) position+=vr_velocity; }
};
}
