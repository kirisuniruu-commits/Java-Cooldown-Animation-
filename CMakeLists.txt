cmake_minimum_required(VERSION 3.22)
project(java_cooldown_animation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PRELOADER_ANDROID_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/third_party/preloader-android"
    CACHE PATH "Path to the preloader-android checkout")
if(NOT EXISTS "${PRELOADER_ANDROID_ROOT}/include/pl/Config.hpp")
  message(FATAL_ERROR
    "Missing ${PRELOADER_ANDROID_ROOT}/include/pl/Config.hpp - copy the "
    "preloader-android source into third_party/preloader-android (see README).")
endif()

include(FetchContent)
set(JCA_DEPS)
if(NOT TARGET nlohmann_json::nlohmann_json)
  FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
  list(APPEND JCA_DEPS nlohmann_json)
endif()
if(NOT TARGET Boost::pfr)
  FetchContent_Declare(boost_pfr
    GIT_REPOSITORY https://github.com/boostorg/pfr.git GIT_TAG boost-1.88.0)
  list(APPEND JCA_DEPS boost_pfr)
endif()
if(NOT TARGET magic_enum::magic_enum)
  FetchContent_Declare(magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git GIT_TAG v0.9.7)
  list(APPEND JCA_DEPS magic_enum)
endif()
if(NOT TARGET fmt::fmt)
  FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 11.2.0)
  set(FMT_DOC OFF CACHE BOOL "" FORCE)
  set(FMT_TEST OFF CACHE BOOL "" FORCE)
  set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
  list(APPEND JCA_DEPS fmt)
endif()
if(JCA_DEPS)
  FetchContent_MakeAvailable(${JCA_DEPS})
endif()

function(jca_common target)
  target_include_directories(${target} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src" "${PRELOADER_ANDROID_ROOT}/include")
  target_link_libraries(${target} PRIVATE
    nlohmann_json::nlohmann_json Boost::pfr magic_enum::magic_enum fmt::fmt)
endfunction()

if(NOT ANDROID)
  add_executable(jca_config_gen src/GenerateConfig.cpp)
  jca_common(jca_config_gen)
  target_compile_definitions(jca_config_gen PRIVATE PL_CONFIG_NO_RUNTIME)

  add_executable(jca_logic_test tests/CooldownLogicTest.cpp)
  target_include_directories(jca_logic_test PRIVATE src)
endif()

if(ANDROID)
  if(NOT TARGET preloader)
    add_subdirectory("${PRELOADER_ANDROID_ROOT}"
                     "${CMAKE_BINARY_DIR}/_preloader_android")
  endif()
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/${ANDROID_ABI}")
  find_library(JCA_LOG_LIB log)

  add_library(java_cooldown_animation SHARED src/JavaCooldownAnimation.cpp)
  jca_common(java_cooldown_animation)
  target_compile_options(java_cooldown_animation PRIVATE
    -fvisibility=hidden -fvisibility-inlines-hidden)
  target_link_libraries(java_cooldown_animation PRIVATE preloader)
  if(JCA_LOG_LIB)
    target_link_libraries(java_cooldown_animation PRIVATE "${JCA_LOG_LIB}")
  endif()
endif()
