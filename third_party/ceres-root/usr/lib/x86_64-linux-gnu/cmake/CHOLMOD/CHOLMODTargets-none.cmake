#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SuiteSparse::CHOLMOD" for configuration "None"
set_property(TARGET SuiteSparse::CHOLMOD APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(SuiteSparse::CHOLMOD PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NONE "SuiteSparse::SuiteSparseConfig;SuiteSparse::AMD;SuiteSparse::COLAMD;SuiteSparse::CAMD;SuiteSparse::CCOLAMD"
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libcholmod.so.5.3.4"
  IMPORTED_SONAME_NONE "libcholmod.so.5"
  )

list(APPEND _cmake_import_check_targets SuiteSparse::CHOLMOD )
list(APPEND _cmake_import_check_files_for_SuiteSparse::CHOLMOD "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libcholmod.so.5.3.4" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
