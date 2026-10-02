#pragma once

namespace OvertureVRInteractionSightPolicy
{
  template<class SightQuery>
  bool IsBetterVisibleCandidate(float distanceSquared,
    float bestDistanceSquared, SightQuery sightQuery)
  {
    return distanceSquared < bestDistanceSquared && sightQuery();
  }

  [[nodiscard]] constexpr bool ShouldRayTestBody(bool abIsCandidate,
    bool abSharesCandidateEntity, bool abCollides)
  {
    if(abIsCandidate) return true;
    if(abSharesCandidateEntity) return false;
    return abCollides;
  }
}
