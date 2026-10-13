#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SuiteSparse::ParU" for configuration "None"
set_property(TARGET SuiteSparse::ParU APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(SuiteSparse::ParU PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NONE "SuiteSparse::SuiteSparseConfig;SuiteSparse::UMFPACK"
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libparu.so.1.1.0"
  IMPORTED_SONAME_NONE "libparu.so.1"
  )

list(APPEND _cmake_import_check_targets SuiteSparse::ParU )
list(APPEND _cmake_import_check_files_for_SuiteSparse::ParU "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libparu.so.1.1.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
