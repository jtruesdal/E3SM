string(APPEND CFLAGS " -mcmodel=medium")
if (compile_threaded)
  string(APPEND CFLAGS " -fopenmp")
endif()
if (DEBUG)
  string(APPEND CFLAGS " -g -Wall -fbacktrace -fcheck=bounds -ffpe-trap=invalid,zero,overflow")
endif()
if (NOT DEBUG)
  string(APPEND CFLAGS " -O")
endif()
if (COMP_NAME STREQUAL csm_share)
  string(APPEND CFLAGS " -std=c99")
endif()
string(APPEND CXXFLAGS " -std=c++14")
if (compile_threaded)
  string(APPEND CXXFLAGS " -fopenmp")
endif()
if (DEBUG)
  string(APPEND CXXFLAGS " -g -Wall -fbacktrace")
endif()
if (NOT DEBUG)
  string(APPEND CXXFLAGS " -O")
endif()
set(CMAKE_AR "/opt/conda/bin/x86_64-conda-linux-gnu-ar")
string(APPEND SPIO_CMAKE_OPTS " -DCMAKE_AR=/opt/conda/bin/x86_64-conda-linux-gnu-ar")
string(APPEND SPIO_CMAKE_OPTS " -DCMAKE_Fortran_COMPILER_RANLIB=/opt/conda/bin/x86_64-conda-linux-gnu-ranlib")
string(APPEND SPIO_CMAKE_OPTS " -DCMAKE_C_COMPILER_RANLIB=/opt/conda/bin/x86_64-conda-linux-gnu-ranlib")
string(APPEND SPIO_CMAKE_OPTS " -DCMAKE_CXX_COMPILER_RANLIB=/opt/conda/bin/x86_64-conda-linux-gnu-ranlib")
string(APPEND CPPDEFS " -DFORTRANUNDERSCORE -DNO_R16 -DCPRGNU")
if (DEBUG)
  string(APPEND CPPDEFS " -DYAKL_DEBUG")
endif()
set(CXX_LIBS "-lstdc++")
string(APPEND FFLAGS " -I/opt/conda/include -mcmodel=medium -fconvert=big-endian -ffree-line-length-none -ffixed-line-length-none")
if (compile_threaded)
  string(APPEND FFLAGS " -fopenmp")
endif()
if (DEBUG)
  string(APPEND FFLAGS " -g -Wall -fbacktrace -fcheck=bounds -ffpe-trap=zero,overflow")
endif()
if (NOT DEBUG)
  string(APPEND FFLAGS " -O")
endif()
string(APPEND FFLAGS_NOOPT " -O0")
string(APPEND FIXEDFLAGS " -ffixed-form")
string(APPEND FREEFLAGS " -ffree-form")
if (compile_threaded)
  string(APPEND LDFLAGS " -fopenmp")
endif()
set(SLIBS " -L/opt/conda/lib")
set(MPICC "/opt/conda/bin/mpicc")
set(MPICXX "/opt/conda/bin/mpicxx")
set(MPIFC "/opt/conda/bin/mpif90")
set(SCC "/opt/conda/bin/x86_64-conda-linux-gnu-gcc")
set(SCXX "/opt/conda/bin/x86_64-conda-linux-gnu-g++")
set(SFC "/opt/conda/bin/x86_64-conda-linux-gnu-gfortran")
