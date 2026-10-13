#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SuiteSparse::SuiteSparseConfig" for configuration "None"
set_property(TARGET SuiteSparse::SuiteSparseConfig APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(SuiteSparse::SuiteSparseConfig PROPERTIES
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libsuitesparseconfig.so.7.12.2"
  IMPORTED_SONAME_NONE "libsuitesparseconfig.so.7"
  )

list(APPEND _cmake_import_check_targets SuiteSparse::SuiteSparseConfig )
list(APPEND _cmake_import_check_files_for_SuiteSparse::SuiteSparseConfig "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libsuitesparseconfig.so.7.12.2" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
