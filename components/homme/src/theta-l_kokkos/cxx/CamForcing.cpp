/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#include "Context.hpp"
#include "CamForcing.hpp"
#include "ForcingFunctor.hpp"
#include "SimulationParams.hpp"
#include "TimeLevel.hpp"
#include "profiling.hpp"

#include "utilities/MathUtils.hpp"

namespace Homme {

static void apply_cam_forcing_tracers(const Real dt, ForcingFunctor& ff,
                                      const TimeLevel& tl,
                                      const SimulationParams& p) {
  GPTLstart("ApplyCAMForcing_tracers");

//original
//  ff.tracers_forcing(dt, tl.n0, tl.n0_qdp, false, p.moisture);

//!!!! adjustment, 
//call for ftype0 + CAM, or any ftype standalone homme
  ff.tracers_forcing(dt, tl.n0, tl.n0_qdp, false, MoistDry::MOIST);

//call for ftype2 + CAM
//  ff.tracers_forcing(dt, tl.n0, tl.n0_qdp, true, MoistDry::MOIST);

  GPTLstop("ApplyCAMForcing_tracers"); 
}

static void apply_cam_forcing_dynamics(const Real dt, ForcingFunctor& ff,
                                       const TimeLevel& tl) {
  GPTLstart("ApplyCAMForcing_dynamics");
  ff.states_forcing(dt, tl.n0);
  GPTLstop("ApplyCAMForcing_dynamics");
}

void apply_cam_forcing(const Real dt) {
  const auto& p  = Context::singleton().get<SimulationParams>();
  const auto& tl = Context::singleton().get<TimeLevel>();
  auto& ff = Context::singleton().get<ForcingFunctor>();
  apply_cam_forcing_tracers(dt, ff, tl, p);
  apply_cam_forcing_dynamics(dt, ff, tl);
}

void apply_cam_forcing_tracers(const Real dt) {
  const auto& p  = Context::singleton().get<SimulationParams>();
  const auto& tl = Context::singleton().get<TimeLevel>();
  auto& ff = Context::singleton().get<ForcingFunctor>();
  apply_cam_forcing_tracers(dt, ff, tl, p);
}

void apply_cam_forcing_dynamics(const Real dt) {
  const auto& tl = Context::singleton().get<TimeLevel>();
  auto& ff = Context::singleton().get<ForcingFunctor>();
  apply_cam_forcing_dynamics(dt, ff, tl);
}

} // namespace Homme
