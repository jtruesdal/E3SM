#ifndef NUDGING_FUNCTIONS_HPP
#define NUDGING_FUNCTIONS_HPP

namespace scream{
namespace nudging{

struct NudgingFunctions
{

  using mPack = ekat::Pack<Real,1>;
  using KT = KokkosTypes<DefaultDevice>;

  template <typename S>
  using view_2d = typename KT::template view_2d<S>;

  template <typename S>
  using view_3d = typename KT::template view_3d<S>;

  struct NudgingData
  {
    
    NudgingData() = default;
    NudgingData(const int ncol_, const int nlev_)
    {
      init(ncol_,nlev_,true);
    }
    void init (const int ncol_, const int nlev_, bool allocate)
    {
      ncols=ncol_;
      nlevs=nlev_;
      if (allocate){
        T_mid = view_2d<Real>("",ncols,nlevs);
        p_mid = view_2d<Real>("",ncols,nlevs);
        hw = view_3d<Real>("",ncols,2,nlevs);
        qv = view_2d<Real>("",ncols,nlevs);
      }
    }
    
    int ncols;
    int nlevs;
    int time;
    view_2d<Real> T_mid;
    view_2d<Real> p_mid;
    view_3d<Real> hw;
    view_2d<Real> qv;
  };


};
}//end of nudging namespace
}//end of scream namespace
  
#endif // NUDGING_FUNCTIONS_HPP
