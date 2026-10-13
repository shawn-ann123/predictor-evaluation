#----------------------------------------------------------------
# Generated CMake target import file for configuration "None".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SuiteSparse::LDL_static" for configuration "None"
set_property(TARGET SuiteSparse::LDL_static APPEND PROPERTY IMPORTED_CONFIGURATIONS NONE)
set_target_properties(SuiteSparse::LDL_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NONE "C"
  IMPORTED_LOCATION_NONE "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libldl.a"
  )

list(APPEND _cmake_import_check_targets SuiteSparse::LDL_static )
list(APPEND _cmake_import_check_files_for_SuiteSparse::LDL_static "${_IMPORT_PREFIX}/lib/x86_64-linux-gnu/libldl.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
