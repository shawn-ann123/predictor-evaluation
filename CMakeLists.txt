cmake_minimum_required(VERSION 3.16)
project(rune_angle_eval LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Daedalus 模拟器 SDK（采集工具用） ----
if(NOT DEFINED DAEDALUS_SDK_ROOT)
  set(DAEDALUS_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../完整版/sdk")
endif()
get_filename_component(DAEDALUS_SDK_ROOT "${DAEDALUS_SDK_ROOT}" ABSOLUTE)
if(NOT EXISTS "${DAEDALUS_SDK_ROOT}/lib/libdaedalus_sim_sdk.a")
  message(FATAL_ERROR "找不到 Daedalus SDK: ${DAEDALUS_SDK_ROOT}")
endif()
add_library(DaedalusSimSdk STATIC IMPORTED)
set_target_properties(DaedalusSimSdk PROPERTIES
  IMPORTED_LOCATION "${DAEDALUS_SDK_ROOT}/lib/libdaedalus_sim_sdk.a"
  INTERFACE_INCLUDE_DIRECTORIES "${DAEDALUS_SDK_ROOT}/include")

find_package(Threads REQUIRED)

# ---- Eigen3 ----
find_package(Eigen3 QUIET)
if(NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "/usr/include/eigen3")
endif()

# ---- 本地 vendored Ceres Solver (third_party/ceres-root) ----
set(CERES_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ceres-root")
set(CERES_VENDOR_LIBDIR "${CERES_VENDOR_ROOT}/usr/lib/x86_64-linux-gnu")
if(NOT EXISTS "${CERES_VENDOR_ROOT}/usr/include/ceres/ceres.h" OR NOT EXISTS "${CERES_VENDOR_LIBDIR}/libceres.so")
  message(FATAL_ERROR "找不到本地 Ceres: ${CERES_VENDOR_ROOT}（见 README 的 third_party 说明）")
endif()
add_library(ceres_vendored INTERFACE)
target_include_directories(ceres_vendored INTERFACE "${CERES_VENDOR_ROOT}/usr/include")
target_link_directories(ceres_vendored INTERFACE "${CERES_VENDOR_LIBDIR}")
target_link_libraries(ceres_vendored INTERFACE ceres glog gflags)
# RPATH（非 RUNPATH）以便 libceres 的传递依赖也能被找到
target_link_options(ceres_vendored INTERFACE "-Wl,--disable-new-dtags" "-Wl,-rpath,${CERES_VENDOR_LIBDIR}")

# ---- 目标 ----
add_executable(capture_rune_angles src/capture_rune_angles.cpp)
target_link_libraries(capture_rune_angles PRIVATE DaedalusSimSdk Threads::Threads)

add_executable(predict_eval src/predict_eval.cpp)
target_link_libraries(predict_eval PRIVATE Eigen3::Eigen ceres_vendored)
