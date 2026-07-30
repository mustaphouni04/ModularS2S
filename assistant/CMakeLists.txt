cmake_minimum_required(VERSION 3.20)
project(modular_s2s_assistant CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(FetchContent)

# --- header-only / source deps fetched at configure time -------------------

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.3
)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# --- system deps -------------------------------------------------------

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)

find_package(Threads REQUIRED)

# --- sources -------------------------------------------------------------

add_library(assistant_core
  src/config/Config.cpp
  src/util/WyomingProtocol.cpp
  src/util/PortAudioGuard.cpp
  src/llm/OllamaClient.cpp
  src/conversation/SentenceBuffer.cpp
  src/conversation/ConversationManager.cpp
  src/tts/WyomingTTSClient.cpp
  src/stt/WyomingSTTClient.cpp
  src/audio/AudioOutput.cpp
  src/audio/AudioInput.cpp
  src/assistant/TypedInputSource.cpp
  src/assistant/VoiceInputSource.cpp
  src/assistant/Assistant.cpp
)

target_include_directories(assistant_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(assistant_core PUBLIC
  httplib::httplib
  nlohmann_json::nlohmann_json
  yaml-cpp::yaml-cpp
  spdlog::spdlog
  PkgConfig::PORTAUDIO
  Threads::Threads
)

add_executable(assistant src/main.cpp)
target_link_libraries(assistant PRIVATE assistant_core)

# --- tests -----------------------------------------------------------------

enable_testing()
add_subdirectory(tests)
