#include "catch2/catch.hpp"

#include "share/io/scream_scorpio_interface.hpp"
#include "physics/spa/spa_functions.hpp"
#include "share/util/scream_time_stamp.hpp"

#include "ekat/ekat_pack.hpp"
#include "ekat/util/ekat_arch.hpp"
#include "ekat/kokkos/ekat_kokkos_utils.hpp"
#include "ekat/ekat_parse_yaml_file.hpp"

#include <algorithm>
#include <array>
#include <random>
#include <thread>

namespace {

using namespace scream;
using namespace spa;

template <typename S>
using view_1d = typename KokkosTypes<DefaultDevice>::template view_1d<S>;
template <typename S>
using view_2d = typename KokkosTypes<DefaultDevice>::template view_2d<S>;
template <typename S>
using view_3d = typename KokkosTypes<DefaultDevice>::template view_3d<S>;

using SPAFunc = spa::SPAFunctions<Real, DefaultDevice>;
using Spack = SPAFunc::Spack;

// Dummy pressure state structure that is non-const for pmid.  Needed to set up the test.
struct PressureState {
  PressureState() = default;
  PressureState(const int ncol_, const int nlev_) :
    ncols(ncol_)
    ,nlevs(nlev_)
  {
    pmid = view_2d<Spack>("",ncols,nlevs);
  }
  // Number of horizontal columns and vertical levels for the data
  Int ncols;
  Int nlevs;
  // Current simulation pressure levels
  view_2d<Spack> pmid;
}; // PressureState

TEST_CASE("spa_read_data","spa")
{
  using C = scream::physics::Constants<Real>;
  static constexpr auto P0 = C::P0;

  // Set up the mpi communicator and init the pio subsystem
  ekat::Comm spa_comm(MPI_COMM_WORLD);  // MPI communicator group used for I/O set as ekat object.
  MPI_Fint fcomm = MPI_Comm_c2f(spa_comm.mpi_comm());  // MPI communicator group used for I/O.  In our simple test we use MPI_COMM_WORLD, however a subset could be used.
  scorpio::eam_init_pio_subsystem(fcomm);   // Gather the initial PIO subsystem data creater by component coupler

  // Establish the SPA function object
  using SPAFunc = spa::SPAFunctions<Real, DefaultDevice>;
  using Spack = SPAFunc::Spack;
  using gid_type = SPAFunc::gid_type;

  std::string fname = "spa_main.yaml";
  ekat::ParameterList test_params("Atmosphere Driver");
  REQUIRE_NOTHROW ( parse_yaml_file(fname,test_params) );
  test_params.print();

  std::string spa_data_file = test_params.get<std::string>("SPA Data File");
  Int ncols    = test_params.get<Int>("ncols");
  Int nlevs    = test_params.get<Int>("nlevs");
  Int nswbands = test_params.get<Int>("nswbands");
  Int nlwbands = test_params.get<Int>("nlwbands");

  // Break the test set of columns into local degrees of freedom per mpi rank
  auto comm_size = spa_comm.size();
  auto comm_rank = spa_comm.rank();

  int my_ncols = ncols/comm_size + (comm_rank < ncols%comm_size ? 1 : 0);
  view_1d<gid_type> dofs_gids("",my_ncols);
  gid_type min_dof = 1; // Start global-ids from 1
  Kokkos::parallel_for("", my_ncols, KOKKOS_LAMBDA(const int& ii) {
    dofs_gids(ii) = min_dof + static_cast<gid_type>(comm_rank + ii*comm_size);
  });
  // Make sure that the total set of columns has been completely broken up.
  Int test_total_ncols = 0;
  spa_comm.all_reduce(&my_ncols,&test_total_ncols,1,MPI_SUM);
  REQUIRE(test_total_ncols == ncols);

  // Set up the set of SPA structures needed to run the test
  SPAFunc::SPAHorizInterp spa_horiz_interp;
  spa_horiz_interp.m_comm = spa_comm;
  SPAFunc::set_remap_weights_one_to_one(ncols,min_dof,dofs_gids,spa_horiz_interp);
  SPAFunc::SPATimeState     spa_time_state;
  PressureState             pressure_state(dofs_gids.size(), nlevs);
  SPAFunc::SPAPressureState spa_pressure_state;
  SPAFunc::SPAData          spa_data_beg(dofs_gids.size(), nlevs+2, nswbands, nlwbands);
  SPAFunc::SPAData          spa_data_end(dofs_gids.size(), nlevs+2, nswbands, nlwbands);
  SPAFunc::SPAOutput        spa_data_out(dofs_gids.size(), nlevs,   nswbands, nlwbands);

  // Verify that the interpolated values match the data, since no temporal or vertical interpolation
  // should be done at this point.
  
  // Create local host views of all relevant data:
  auto hyam_h         = Kokkos::create_mirror_view(spa_data_beg.hyam);
  auto hybm_h         = Kokkos::create_mirror_view(spa_data_beg.hybm);
  // Beg data for time interpolation
  auto ps_beg         = Kokkos::create_mirror_view(spa_data_beg.PS);
  auto ccn3_beg       = Kokkos::create_mirror_view(spa_data_beg.CCN3);
  auto aer_g_sw_beg   = Kokkos::create_mirror_view(spa_data_beg.AER_G_SW);
  auto aer_ssa_sw_beg = Kokkos::create_mirror_view(spa_data_beg.AER_SSA_SW);
  auto aer_tau_sw_beg = Kokkos::create_mirror_view(spa_data_beg.AER_TAU_SW);
  auto aer_tau_lw_beg = Kokkos::create_mirror_view(spa_data_beg.AER_TAU_LW);
  // End data for time interpolation
  auto ps_end         = Kokkos::create_mirror_view(spa_data_end.PS);
  auto ccn3_end       = Kokkos::create_mirror_view(spa_data_end.CCN3);
  auto aer_g_sw_end   = Kokkos::create_mirror_view(spa_data_end.AER_G_SW);
  auto aer_ssa_sw_end = Kokkos::create_mirror_view(spa_data_end.AER_SSA_SW);
  auto aer_tau_sw_end = Kokkos::create_mirror_view(spa_data_end.AER_TAU_SW);
  auto aer_tau_lw_end = Kokkos::create_mirror_view(spa_data_end.AER_TAU_LW);
  // Output
  auto ccn3_h       = Kokkos::create_mirror_view(spa_data_out.CCN3);
  auto aer_g_sw_h   = Kokkos::create_mirror_view(spa_data_out.AER_G_SW);
  auto aer_ssa_sw_h = Kokkos::create_mirror_view(spa_data_out.AER_SSA_SW);
  auto aer_tau_sw_h = Kokkos::create_mirror_view(spa_data_out.AER_TAU_SW);
  auto aer_tau_lw_h = Kokkos::create_mirror_view(spa_data_out.AER_TAU_LW);
  
  // First initialize the start and end month data:  Set for January
  util::TimeStamp ts(1900,1,1,0,0,0);
  SPAFunc::update_spa_timestate(spa_data_file,nswbands,nlwbands,ts,spa_horiz_interp,spa_time_state,spa_data_beg,spa_data_end);

  Kokkos::deep_copy(hyam_h        ,spa_data_beg.hyam);
  Kokkos::deep_copy(hybm_h        ,spa_data_beg.hybm);
  Kokkos::deep_copy(ps_beg        ,spa_data_beg.PS);
  Kokkos::deep_copy(ccn3_beg      ,spa_data_beg.CCN3);
  Kokkos::deep_copy(aer_g_sw_beg  ,spa_data_beg.AER_G_SW);
  Kokkos::deep_copy(aer_ssa_sw_beg,spa_data_beg.AER_SSA_SW);
  Kokkos::deep_copy(aer_tau_sw_beg,spa_data_beg.AER_TAU_SW);
  Kokkos::deep_copy(aer_tau_lw_beg,spa_data_beg.AER_TAU_LW);
  Kokkos::deep_copy(ps_end        ,spa_data_end.PS);
  Kokkos::deep_copy(ccn3_end      ,spa_data_end.CCN3);
  Kokkos::deep_copy(aer_g_sw_end  ,spa_data_end.AER_G_SW);
  Kokkos::deep_copy(aer_ssa_sw_end,spa_data_end.AER_SSA_SW);
  Kokkos::deep_copy(aer_tau_sw_end,spa_data_end.AER_TAU_SW);
  Kokkos::deep_copy(aer_tau_lw_end,spa_data_end.AER_TAU_LW);

  // Create the pressure state.  Note, we need to create the pmid values for the actual data.  We will build it based on the PS and hya/bm
  // coordinates in the beginning data.
  auto dofs_gids_h = Kokkos::create_mirror_view(dofs_gids);
  Kokkos::deep_copy(dofs_gids_h,dofs_gids);
  spa_pressure_state.ncols = dofs_gids.size(); 
  spa_pressure_state.nlevs = nlevs;

  spa_pressure_state.pmid  = pressure_state.pmid;
  auto pmid_h = Kokkos::create_mirror_view(pressure_state.pmid);

  // Note, hyam and hybm are padded
  for (size_t dof_i=0;dof_i<dofs_gids_h.size();dof_i++) {
    for (int kk=0;kk<nlevs;kk++) {
      int kpack = kk / Spack::n;
      int kidx  = kk % Spack::n;
      int kpack_pad = (kk+1) / Spack::n;
      int kidx_pad  = (kk+1) % Spack::n;
      pmid_h(dof_i,kpack)[kidx] = ps_beg(dof_i)*hybm_h(kpack_pad)[kidx_pad] + P0*hyam_h(kpack_pad)[kidx_pad];
    }
  }
  int kpack_pad = (nlevs+1) / Spack::n;
  int kidx_pad  = (nlevs+1) % Spack::n;
  Kokkos::deep_copy(pressure_state.pmid,pmid_h);

  // Run SPA main
  SPAFunc::spa_main(spa_time_state,spa_pressure_state,spa_data_beg,spa_data_end,spa_data_out,dofs_gids.size(),nlevs,nswbands,nlwbands);

  Kokkos::deep_copy(ccn3_h      , spa_data_out.CCN3);
  Kokkos::deep_copy(aer_g_sw_h  , spa_data_out.AER_G_SW);
  Kokkos::deep_copy(aer_ssa_sw_h, spa_data_out.AER_SSA_SW);
  Kokkos::deep_copy(aer_tau_sw_h, spa_data_out.AER_TAU_SW);
  Kokkos::deep_copy(aer_tau_lw_h, spa_data_out.AER_TAU_LW);

  // The output data should match the input data exactly since there is no time interpolation
  // and the pmid profile should match the constructed profile within spa_main.
  for (size_t dof_i=0;dof_i<dofs_gids_h.size();dof_i++) {
    for (int kk=0;kk<nlevs;kk++) {
      int kpack = kk / Spack::n;
      int kidx  = kk % Spack::n;
      int kpack_pad = (kk+1) / Spack::n;
      int kidx_pad  = (kk+1) % Spack::n;
      REQUIRE(ccn3_h(dof_i,kpack)[kidx] == ccn3_beg(dof_i,kpack_pad)[kidx_pad] );
      for (int n=0;n<nswbands;n++) {
        REQUIRE(aer_g_sw_h(dof_i,n,kpack)[kidx]   == aer_g_sw_beg(dof_i,n,kpack_pad)[kidx_pad]);
        REQUIRE(aer_ssa_sw_h(dof_i,n,kpack)[kidx] == aer_ssa_sw_beg(dof_i,n,kpack_pad)[kidx_pad] );
        REQUIRE(aer_tau_sw_h(dof_i,n,kpack)[kidx] == aer_tau_sw_beg(dof_i,n,kpack_pad)[kidx_pad] );
      }
      for (int n=0;n<nlwbands;n++) {
        REQUIRE(aer_tau_lw_h(dof_i,n,kpack)[kidx] == aer_tau_lw_beg(dof_i,n,kpack_pad)[kidx_pad] );
      }
    }
  }

  // Add a month and recalculate.  Should now match the end of the month data.
  ts += util::days_in_month(ts.get_year(),ts.get_month())*86400;
  spa_time_state.t_now = ts.frac_of_year_in_days();

  for (size_t dof_i=0;dof_i<dofs_gids_h.size();dof_i++) {
    for (int kk=0;kk<nlevs;kk++) {
      int kpack = kk / Spack::n;
      int kidx  = kk % Spack::n;
      int kpack_pad = (kk+1) / Spack::n;
      int kidx_pad  = (kk+1) % Spack::n;
      pmid_h(dof_i,kpack)[kidx] = ps_end(dof_i)*hybm_h(kpack_pad)[kidx_pad] + P0*hyam_h(kpack_pad)[kidx_pad];
    }
  }
  Kokkos::deep_copy(pressure_state.pmid,pmid_h);
  
  SPAFunc::spa_main(spa_time_state,spa_pressure_state,spa_data_beg,spa_data_end,spa_data_out,dofs_gids.size(),nlevs,nswbands,nlwbands);
  Kokkos::deep_copy(ccn3_h      , spa_data_out.CCN3);
  Kokkos::deep_copy(aer_g_sw_h  , spa_data_out.AER_G_SW);
  Kokkos::deep_copy(aer_ssa_sw_h, spa_data_out.AER_SSA_SW);
  Kokkos::deep_copy(aer_tau_sw_h, spa_data_out.AER_TAU_SW);
  Kokkos::deep_copy(aer_tau_lw_h, spa_data_out.AER_TAU_LW);

  // The output data should match the input data exactly since there is no time interpolation
  // and the pmid profile should match the constructed profile within spa_main.
  for (size_t dof_i=0;dof_i<dofs_gids_h.size();dof_i++) {
    for (int kk=0;kk<nlevs;kk++) {
      int kpack     = kk / Spack::n;
      int kidx      = kk % Spack::n;
      int kpack_pad = (kk+1) / Spack::n;
      int kidx_pad  = (kk+1) % Spack::n;
      REQUIRE(ccn3_h(dof_i,kpack)[kidx] == ccn3_end(dof_i,kpack_pad)[kidx_pad] );
      for (int n=0;n<nswbands;n++) {
        REQUIRE(aer_g_sw_h(dof_i,n,kpack)[kidx]   == aer_g_sw_end(dof_i,n,kpack_pad)[kidx_pad]   );
        REQUIRE(aer_ssa_sw_h(dof_i,n,kpack)[kidx] == aer_ssa_sw_end(dof_i,n,kpack_pad)[kidx_pad] );
        REQUIRE(aer_tau_sw_h(dof_i,n,kpack)[kidx] == aer_tau_sw_end(dof_i,n,kpack_pad)[kidx_pad] );
      }
      for (int n=0;n<nlwbands;n++) {
        REQUIRE(aer_tau_lw_h(dof_i,n,kpack)[kidx] == aer_tau_lw_end(dof_i,n,kpack_pad)[kidx_pad] );
      }
    }
  }

  // Add a few days and update spa data.  Make sure that the output values are not outside of the bounds
  // of the actual SPA data 
  ts += (int)(util::days_in_month(ts.get_year(),ts.get_month())*0.5)*86400;
  SPAFunc::update_spa_timestate(spa_data_file,nswbands,nlwbands,ts,spa_horiz_interp,spa_time_state,spa_data_beg,spa_data_end);
  Kokkos::deep_copy(ps_beg        ,spa_data_beg.PS);
  Kokkos::deep_copy(ccn3_beg      ,spa_data_beg.CCN3);
  Kokkos::deep_copy(aer_g_sw_beg  ,spa_data_beg.AER_G_SW);
  Kokkos::deep_copy(aer_ssa_sw_beg,spa_data_beg.AER_SSA_SW);
  Kokkos::deep_copy(aer_tau_sw_beg,spa_data_beg.AER_TAU_SW);
  Kokkos::deep_copy(aer_tau_lw_beg,spa_data_beg.AER_TAU_LW);
  Kokkos::deep_copy(ps_end        ,spa_data_end.PS);
  Kokkos::deep_copy(ccn3_end      ,spa_data_end.CCN3);
  Kokkos::deep_copy(aer_g_sw_end  ,spa_data_end.AER_G_SW);
  Kokkos::deep_copy(aer_ssa_sw_end,spa_data_end.AER_SSA_SW);
  Kokkos::deep_copy(aer_tau_sw_end,spa_data_end.AER_TAU_SW);
  Kokkos::deep_copy(aer_tau_lw_end,spa_data_end.AER_TAU_LW);
  // Create a target pressure profile to interpolate onto that has a slightly higher surface pressure that the bounds.
  // This will force extrapolation.
  for (size_t dof_i=0;dof_i<dofs_gids_h.size();dof_i++) {
    for (int kk=0;kk<nlevs;kk++) {
      int kpack     = kk / Spack::n;
      int kidx      = kk % Spack::n;
      int kpack_pad = (kk+1) / Spack::n;
      int kidx_pad  = (kk+1) % Spack::n;
      Real ps = 1.05*std::max(ps_beg(dof_i),ps_end(dof_i));
      pmid_h(dof_i,kpack)[kidx] = ps*hybm_h(kpack_pad)[kidx_pad] + P0*hyam_h(kpack_pad)[kidx_pad];
    }
  }
  Kokkos::deep_copy(pressure_state.pmid,pmid_h);

  SPAFunc::spa_main(spa_time_state,spa_pressure_state,spa_data_beg,spa_data_end,spa_data_out,dofs_gids.size(),nlevs,nswbands,nlwbands);
  Kokkos::deep_copy(ccn3_h      , spa_data_out.CCN3);
  Kokkos::deep_copy(aer_g_sw_h  , spa_data_out.AER_G_SW);
  Kokkos::deep_copy(aer_ssa_sw_h, spa_data_out.AER_SSA_SW);
  Kokkos::deep_copy(aer_tau_sw_h, spa_data_out.AER_TAU_SW);
  Kokkos::deep_copy(aer_tau_lw_h, spa_data_out.AER_TAU_LW);

  // Calculate the min and max values for all spa input data for all columns 
  auto ccn3_bnds       = view_2d<Real>("",2,ncols);
  auto aer_sw_g_bnds   = view_3d<Real>("",2,ncols,nswbands);
  auto aer_sw_ssa_bnds = view_3d<Real>("",2,ncols,nswbands);
  auto aer_sw_tau_bnds = view_3d<Real>("",2,ncols,nswbands);
  auto aer_lw_tau_bnds = view_3d<Real>("",2,ncols,nlwbands);
  auto ccn3_bnds_h       = Kokkos::create_mirror_view(ccn3_bnds      );
  auto aer_sw_g_bnds_h   = Kokkos::create_mirror_view(aer_sw_g_bnds  );
  auto aer_sw_ssa_bnds_h = Kokkos::create_mirror_view(aer_sw_ssa_bnds);
  auto aer_sw_tau_bnds_h = Kokkos::create_mirror_view(aer_sw_tau_bnds);
  auto aer_lw_tau_bnds_h = Kokkos::create_mirror_view(aer_lw_tau_bnds);
  for (size_t dof_i=0;dof_i<dofs_gids_h.size();dof_i++) {
    for (int kk=0;kk<nlevs;kk++) {
      int kpack_pad = (kk+1) / Spack::n;
      int kidx_pad  = (kk+1) % Spack::n;
      Real tmp_min, tmp_max;
      tmp_min = std::min(ccn3_beg(dof_i,kpack_pad)[kidx_pad],ccn3_end(dof_i,kpack_pad)[kidx_pad]);
      tmp_max = std::max(ccn3_beg(dof_i,kpack_pad)[kidx_pad],ccn3_end(dof_i,kpack_pad)[kidx_pad]);
      if (kk == 0) {
        ccn3_bnds_h(0,dof_i) = tmp_min; 
        ccn3_bnds_h(1,dof_i) = tmp_max;
      } else {
        ccn3_bnds_h(0,dof_i) = std::min(ccn3_bnds_h(0,dof_i), tmp_min);
        ccn3_bnds_h(1,dof_i) = std::max(ccn3_bnds_h(1,dof_i), tmp_max);
      }
      for (int n=0;n<nswbands;n++) {
        Real tmp_min_g, tmp_max_g;
        tmp_min_g = std::min(aer_g_sw_beg(dof_i,n,kpack_pad)[kidx_pad],aer_g_sw_end(dof_i,n,kpack_pad)[kidx_pad]);
        tmp_max_g = std::max(aer_g_sw_beg(dof_i,n,kpack_pad)[kidx_pad],aer_g_sw_end(dof_i,n,kpack_pad)[kidx_pad]);
        Real tmp_min_ssa, tmp_max_ssa;
        tmp_min_ssa = std::min(aer_ssa_sw_beg(dof_i,n,kpack_pad)[kidx_pad],aer_ssa_sw_end(dof_i,n,kpack_pad)[kidx_pad]);
        tmp_max_ssa = std::max(aer_ssa_sw_beg(dof_i,n,kpack_pad)[kidx_pad],aer_ssa_sw_end(dof_i,n,kpack_pad)[kidx_pad]);
        Real tmp_min_tau, tmp_max_tau;
        tmp_min_tau = std::min(aer_tau_sw_beg(dof_i,n,kpack_pad)[kidx_pad],aer_tau_sw_end(dof_i,n,kpack_pad)[kidx_pad]);
        tmp_max_tau = std::max(aer_tau_sw_beg(dof_i,n,kpack_pad)[kidx_pad],aer_tau_sw_end(dof_i,n,kpack_pad)[kidx_pad]);

        if (kk == 0) {
          aer_sw_g_bnds_h(0,dof_i,n)   = tmp_min_g; 
          aer_sw_g_bnds_h(1,dof_i,n)   = tmp_max_g;
          aer_sw_tau_bnds_h(0,dof_i,n) = tmp_min_tau;
          aer_sw_tau_bnds_h(1,dof_i,n) = tmp_max_tau;
          aer_sw_ssa_bnds_h(0,dof_i,n) = tmp_min_ssa;
          aer_sw_ssa_bnds_h(1,dof_i,n) = tmp_max_ssa;
        } else {
          aer_sw_g_bnds_h(0,dof_i,n)   = std::min(aer_sw_g_bnds_h(0,dof_i,n),  tmp_min_g); 
          aer_sw_g_bnds_h(1,dof_i,n)   = std::max(aer_sw_g_bnds_h(1,dof_i,n),  tmp_max_g);
          aer_sw_tau_bnds_h(0,dof_i,n) = std::min(aer_sw_tau_bnds_h(0,dof_i,n),tmp_min_tau);
          aer_sw_tau_bnds_h(1,dof_i,n) = std::max(aer_sw_tau_bnds_h(1,dof_i,n),tmp_max_tau);
          aer_sw_ssa_bnds_h(0,dof_i,n) = std::min(aer_sw_ssa_bnds_h(0,dof_i,n),tmp_min_ssa);
          aer_sw_ssa_bnds_h(1,dof_i,n) = std::max(aer_sw_ssa_bnds_h(1,dof_i,n),tmp_max_ssa);
        }
      }
      for (int n=0;n<nlwbands;n++) {
        Real tmp_min_tau, tmp_max_tau;
        tmp_min_tau = std::min(aer_tau_lw_beg(dof_i,n,kpack_pad)[kidx_pad],aer_tau_lw_end(dof_i,n,kpack_pad)[kidx_pad]);
        tmp_max_tau = std::max(aer_tau_lw_beg(dof_i,n,kpack_pad)[kidx_pad],aer_tau_lw_end(dof_i,n,kpack_pad)[kidx_pad]);
        if (kk == 0) {
          aer_lw_tau_bnds_h(0,dof_i,n) = tmp_min_tau;
          aer_lw_tau_bnds_h(1,dof_i,n) = tmp_max_tau;
        } else {
          aer_lw_tau_bnds_h(0,dof_i,n) = std::min(aer_lw_tau_bnds_h(0,dof_i,n), tmp_min_tau);
          aer_lw_tau_bnds_h(1,dof_i,n) = std::max(aer_lw_tau_bnds_h(1,dof_i,n), tmp_max_tau);
        }
      }
    }
  }

  // Make sure the output data is within the same bounds as the input data.
  Real tol = 1.1;
  for (size_t dof_i=0;dof_i<dofs_gids_h.size();dof_i++) {
    for (int kk=0;kk<nlevs;kk++) {
      int kpack = kk / Spack::n;
      int kidx  = kk % Spack::n;
      REQUIRE(ccn3_h(dof_i,kpack)[kidx]>=tol*ccn3_bnds_h(0,dof_i));
      REQUIRE(ccn3_h(dof_i,kpack)[kidx]<=tol*ccn3_bnds_h(1,dof_i));
      for (int n=0;n<nswbands;n++) {
        REQUIRE(aer_g_sw_h(dof_i,n,kpack)[kidx]   >= tol*aer_sw_g_bnds_h  (0,dof_i,n) );
        REQUIRE(aer_ssa_sw_h(dof_i,n,kpack)[kidx] >= tol*aer_sw_ssa_bnds_h(0,dof_i,n) );
        REQUIRE(aer_tau_sw_h(dof_i,n,kpack)[kidx] >= tol*aer_sw_tau_bnds_h(0,dof_i,n) );
        REQUIRE(aer_g_sw_h(dof_i,n,kpack)[kidx]   <= tol*aer_sw_g_bnds_h  (1,dof_i,n) );
        REQUIRE(aer_ssa_sw_h(dof_i,n,kpack)[kidx] <= tol*aer_sw_ssa_bnds_h(1,dof_i,n) );
        REQUIRE(aer_tau_sw_h(dof_i,n,kpack)[kidx] <= tol*aer_sw_tau_bnds_h(1,dof_i,n) );
      }
      for (int n=0;n<nlwbands;n++) {
        REQUIRE(aer_tau_lw_h(dof_i,n,kpack)[kidx] >= tol*aer_lw_tau_bnds_h(0,dof_i,n) );
        REQUIRE(aer_tau_lw_h(dof_i,n,kpack)[kidx] <= tol*aer_lw_tau_bnds_h(1,dof_i,n) );
      }
    }
  }




  // All Done 
  scorpio::eam_pio_finalize();
} // run_property

} // namespace

