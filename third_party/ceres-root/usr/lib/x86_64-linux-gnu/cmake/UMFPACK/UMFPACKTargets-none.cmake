#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SuiteSparse::UMFPACK" for configuration "None"
set_property(TARGET SuiteSparse::UMFPACK APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(SuiteSparse::UMFPACK PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NONE "SuiteSparse::SuiteSparseConfig;SuiteSparse::AMD;SuiteSparse::CHOLMOD"
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libumfpack.so.6.3.7"
  IMPORTED_SONAME_NONE "libumfpack.so.6"
  )

list(APPEND _cmake_import_check_targets SuiteSparse::UMFPACK )
list(APPEND _cmake_import_check_files_for_SuiteSparse::UMFPACK "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libumfpack.so.6.3.7" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
