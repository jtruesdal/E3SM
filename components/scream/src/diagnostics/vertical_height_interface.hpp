#ifndef EAMXX_VERTICAL_INT_HGHT_DIAGNOSTIC_HPP
#define EAMXX_VERTICAL_INT_HGHT_DIAGNOSTIC_HPP

#include "share/atm_process/atmosphere_diagnostic.hpp"
#include "share/util/scream_common_physics_functions.hpp"
#include "ekat/kokkos/ekat_subview_utils.hpp"

namespace scream
{

/*
 * This diagnostic will produce the potential temperature.
 */

class VerticalInterfaceHeightDiagnostic : public AtmosphereDiagnostic
{
public:
  template <typename S>
  using SmallPack     = ekat::Pack<S,SCREAM_SMALL_PACK_SIZE>;

  using Spack         = SmallPack<Real>;
  using Pack          = ekat::Pack<Real,Spack::n>;
  using PF            = scream::PhysicsFunctions<DefaultDevice>;
  using KT            = KokkosTypes<DefaultDevice>;
  using MemberType    = typename KT::MemberType;
  using view_1d       = typename KT::template view_1d<Spack>;
  using view_2d       = typename KT::template view_2d<Spack>;
  using view_2d_const = typename KT::template view_2d<const Spack>;

  template<typename ScalarT>
  using uview_1d = Unmanaged<typename KT::template view_1d<ScalarT>>;
  template<typename ScalarT>
  using uview_2d = Unmanaged<typename KT::template view_2d<ScalarT>>;

  // Constructors
  VerticalInterfaceHeightDiagnostic (const ekat::Comm& comm, const ekat::ParameterList& params);

  // Set type to diagnostic
  AtmosphereProcessType type () const { return AtmosphereProcessType::Diagnostic; }

  // The name of the diagnostic
  std::string name () const { return "Potential Temperature"; } 

  // Get the required grid for the diagnostic
  std::set<std::string> get_required_grids () const {
    static std::set<std::string> s;
    s.insert(m_params.get<std::string>("Grid"));
    return s;
  }

  // Set the grid
  void set_grids (const std::shared_ptr<const GridsManager> grids_manager);

  // Actual diagnostic calculation 
  struct run_diagnostic_impl {
    run_diagnostic_impl() = default;
    // Functor for Kokkos loop
    KOKKOS_INLINE_FUNCTION
    void operator() (const MemberType& team) const {
      const int icol = team.league_rank();
      const auto& T_mid_i      = ekat::subview(T_mid,icol);
      const auto& p_mid_i      = ekat::subview(p_mid,icol);
      const auto& qv_mid_i     = ekat::subview(qv_mid,icol);
      const auto& pseudo_mid_i = ekat::subview(pseudo_density_mid,icol);
      PF::calculate_dz(team,pseudo_mid_i, p_mid_i, T_mid_i, qv_mid_i, dz_i);
      const Real z_surf    = 0.0; // TODO, add this to cpp instead of in hpp hard-coded.
      const auto& output_i = ekat::subview(output, icol);
      PF::calculate_z_int(team,m_nlevs,dz_i,z_surf,output_i);
    }
    int m_ncol, m_nlevs;
    view_2d_const T_mid;
    view_2d_const p_mid;
    view_2d_const qv_mid;
    view_2d_const pseudo_density_mid;
    view_2d       output;
    view_2d       dz_i;
    // assign variables to this structure
    void set_variables(const int ncol, const int nlevs,
      const view_2d_const& pmid_, const view_2d_const& tmid_,
      const view_2d_const& qvmid_, const view_2d_const& pseudo_density_mid_, 
      const view_2d& output_)
    {
      m_ncol   = ncol;
      m_nlevs  = nlevs;
      // IN
      T_mid = tmid_;
      p_mid = pmid_;
      qv_mid = qvmid_;
      pseudo_density_mid = pseudo_density_mid_;
      // OUT
      output = output_;
    }
  }; // struct run_diagnostic_impl

protected:

  // The three main overrides for the subcomponent
  void initialize_impl (const RunType run_type);
  void run_impl        (const int dt);
  void finalize_impl   ();

  // Keep track of field dimensions
  Int m_num_cols; 
  Int m_num_levs;

  // Structure to run the diagnostic
  run_diagnostic_impl  run_diagnostic;

}; // class VerticalInterfaceHeightDiagnostic

} //namespace scream

#endif // EAMXX_VERTICAL_INT_HGHT_DIAGNOSTIC_HPP
