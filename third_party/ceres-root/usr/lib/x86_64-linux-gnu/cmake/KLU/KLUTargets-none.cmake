#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SuiteSparse::KLU" for configuration "None"
set_property(TARGET SuiteSparse::KLU APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(SuiteSparse::KLU PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NONE "SuiteSparse::SuiteSparseConfig;SuiteSparse::AMD;SuiteSparse::COLAMD;SuiteSparse::BTF"
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libklu.so.2.3.6"
  IMPORTED_SONAME_NONE "libklu.so.2"
  )

list(APPEND _cmake_import_check_targets SuiteSparse::KLU )
list(APPEND _cmake_import_check_files_for_SuiteSparse::KLU "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libklu.so.2.3.6" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
