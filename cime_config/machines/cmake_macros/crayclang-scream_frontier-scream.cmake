if (compile_threaded)
  #string(APPEND CFLAGS " -fopenmp")
  string(APPEND FFLAGS " -fopenmp")
  string(APPEND CXXFLAGS " -fopenmp")
  string(APPEND LDFLAGS " -fopenmp")
endif()
if (COMP_NAME STREQUAL elm)
  string(APPEND FFLAGS " -hfp0")
endif()
string(APPEND FFLAGS " -hipa0 -hzero -hsystem_alloc -f free -N 255 -h byteswapio")
string(APPEND FFLAGS " -em -ef")

string(APPEND SLIBS " -L$ENV{PNETCDF_PATH}/lib -lpnetcdf")
string(APPEND SLIBS " -L$ENV{ROCM_PATH}/lib -lamdhip64 $ENV{OLCF_LIBUNWIND_ROOT}/lib/libunwind.a /sw/frontier/spack-envs/base/opt/cray-sles15-zen3/clang-14.0.0-rocm5.2.0/gperftools-2.10-6g5acp4pcilrl62tddbsbxlut67pp7qn/lib/libtcmalloc.a")

set(NETCDF_PATH "$ENV{NETCDF_DIR}")
set(PNETCDF_PATH "$ENV{PNETCDF_DIR}")
set(PIO_FILESYSTEM_HINTS "gpfs")
string(APPEND CXX_LIBS " -lstdc++")
