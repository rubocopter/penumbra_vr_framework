#pragma once
#define OPENVR_INTERFACE_INTERNAL
#include "../../../../products/overture/dependencies/openvr-2.15.6/headers/openvr.h"
namespace vr {
extern IVRInput* audit_input;
extern IVRCompositor* audit_compositor;
extern IVRSystem* audit_system;
inline IVRInput* VRInput() { return audit_input; }
inline IVRCompositor* VRCompositor() { return audit_compositor; }
inline IVRSystem* VRSystem() { return audit_system; }
}
