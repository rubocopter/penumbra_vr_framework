#pragma once
#include "physics/CharacterBody.h"
struct cPlayer {
    hpl::iCharacterBody* body=nullptr;
    hpl::cVector3f vr_headPos{0.0f}, vr_moveVec{0.0f};
    int jumps=0, held_writes=0;
    bool jump_held=false;
    hpl::iCharacterBody* GetCharacterBody() const { return body; }
    void Jump() { ++jumps; }
    void SetJumpButtonDown(bool down) { ++held_writes; jump_held=down; }
};
