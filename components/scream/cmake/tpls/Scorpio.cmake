# If this is a CIME build, create IMPORTED target to wrap scorpio libs.
# Otherwise, simply add scorpio subdirectory.
set (E3SM_EXTERNALS_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../externals CACHE INTERNAL "")

set (SCREAM_TPLS_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")
include (${SCREAM_TPLS_MODULE_DIR}/GPTL.cmake)
include (${SCREAM_TPLS_MODULE_DIR}/GetNetcdfLibs.cmake)

macro (CreateScorpioTargets)

  # If we already parsed this script, then pioc/piof are already targets
  if (NOT TARGET pioc)
    # Sanity check
    if (TARGET piof)
      message (FATAL_ERROR "Something is off: pioc ws not yet created but piof was.")
    endif()

    if (SCREAM_CIME_BUILD)
      # For CIME builds, we simply wrap the already built pioc/piof libs into a cmake target
      if (NOT DEFINED INSTALL_SHAREDPATH)
        message (FATAL_ERROR "Error! The cmake variable 'INSTALL_SHAREDPATH' is not defined.")
      endif ()

      set(SCORPIO_LIB_DIR ${INSTALL_SHAREDPATH}/lib)
      set(SCORPIO_INC_DIR ${INSTALL_SHAREDPATH}/include)
      set(CSM_SHR_INCLUDE ${INSTALL_SHAREDPATH}/${COMP_INTERFACE}/noesmf/${NINST_VALUE}/include)

      # Look for pioc deps. We will have to link them to the pioc target, so that cmake will
      # propagate them to any downstream target linking against pioc
      CreateGPTLTarget()
      GetNetcdfLibs()

      ######################
      #        PIOc        #
      ######################

      # Look for pioc in INSTALL_SHAREDPATH/lib
      find_library(SCORPIO_C_LIB pioc REQUIRED PATHS ${SCORPIO_LIB_DIR})

      # Create the interface library, and set target properties
      add_library (pioc INTERFACE)
      target_link_libraries (pioc INTERFACE ${SCORPIO_C_LIB} gptl ${netcdf_c_lib})
      target_include_directories (pioc INTERFACE ${SCORPIO_INC_DIR} ${CSM_SHR_INCLUDE})
      if (pnetcdf_lib)
        target_link_libraries(pioc INTERFACE "${pnetcdf_lib}")
      endif ()

      ######################
      #        PIOf        #
      ######################

      # Look for piof lib in INSTALL_SHAREDPATH/lib
      find_library(SCORPIO_F_LIB piof REQUIRED PATHS ${INSTALL_SHAREDPATH}/lib)

      # Create the interface library, and set target properties
      add_library(piof INTERFACE)
      target_link_libraries (pioc INTERFACE ${SCORPIO_F_LIB} ${netcdf_f_lib} pioc)
      target_include_directories (pioc INTERFACE ${SCORPIO_INC_DIR})

    else ()
      # Not a CIME build. We'll add scorpio as a subdir

      # We don't need (yet) SCORPIO tools
      option (PIO_ENABLE_TOOLS "Enable SCORPIO tools" OFF)

      # We want to use GPTL internally
      option (PIO_ENABLE_TIMING    "Enable the use of the GPTL timing library" ON)

      # This is the default, but just in case scorpio changes it
      option (PIO_ENABLE_FORTRAN "Enable the Fortran library builds" ON)

      add_subdirectory (${E3SM_EXTERNALS_DIR}/scorpio ${CMAKE_BINARY_DIR}/externals/scorpio)
      EkatDisableAllWarning(pioc)
      EkatDisableAllWarning(piof)
      EkatDisableAllWarning(gptl)
    endif ()
  endif ()
endmacro()
