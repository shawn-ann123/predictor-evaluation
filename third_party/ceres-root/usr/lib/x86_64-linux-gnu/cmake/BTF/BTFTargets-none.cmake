#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SuiteSparse::BTF" for configuration "None"
set_property(TARGET SuiteSparse::BTF APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(SuiteSparse::BTF PROPERTIES
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libbtf.so.2.3.3"
  IMPORTED_SONAME_NONE "libbtf.so.2"
  )

list(APPEND _cmake_import_check_targets SuiteSparse::BTF )
list(APPEND _cmake_import_check_files_for_SuiteSparse::BTF "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libbtf.so.2.3.3" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
