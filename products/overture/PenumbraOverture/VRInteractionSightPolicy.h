#pragma once

namespace OvertureVRInteractionSightPolicy
{
  [[nodiscard]] constexpr bool ShouldRayTestBody(bool abIsCandidate,
    bool abSharesCandidateEntity, bool abCollides)
  {
    if(abIsCandidate) return true;
    if(abSharesCandidateEntity) return false;
    return abCollides;
  }
}
