#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SuiteSparse::COLAMD" for configuration "None"
set_property(TARGET SuiteSparse::COLAMD APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(SuiteSparse::COLAMD PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NONE "SuiteSparse::SuiteSparseConfig"
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libcolamd.so.3.3.5"
  IMPORTED_SONAME_NONE "libcolamd.so.3"
  )

list(APPEND _cmake_import_check_targets SuiteSparse::COLAMD )
list(APPEND _cmake_import_check_files_for_SuiteSparse::COLAMD "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libcolamd.so.3.3.5" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
