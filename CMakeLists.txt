cmake_minimum_required(VERSION 3.20)
project(SimpleSSHTerm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

# Optional deps: libssh, libsodium
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBSSH libssh)
pkg_check_modules(SODIUM libsodium)
pkg_check_modules(LIBVTERM vterm)

if (NOT LIBVTERM_FOUND)
  # Some installs may provide libvterm.pc; try that as a fallback
  pkg_check_modules(LIBVTERM libvterm)
endif()

if (NOT LIBVTERM_FOUND)
  find_path(LIBVTERM_INCLUDE_DIR vterm.h PATHS /opt/local/include /usr/local/include)
  find_library(LIBVTERM_LIBRARY vterm PATHS /opt/local/lib /usr/local/lib)
  if (LIBVTERM_INCLUDE_DIR AND LIBVTERM_LIBRARY)
    set(LIBVTERM_FOUND ON)
    set(LIBVTERM_INCLUDE_DIRS ${LIBVTERM_INCLUDE_DIR})
    set(LIBVTERM_LIBRARIES ${LIBVTERM_LIBRARY})
  endif()
endif()

add_executable(SimpleSSHTerm
  src/main.cpp
  src/MainWindow.cpp
  src/TerminalTab.cpp
  src/TerminalWidget.cpp
  src/SshSession.cpp
  src/ProfileStore.cpp
  src/ProfileManagerDialog.cpp
  src/ThemeDialog.cpp
  include/MainWindow.h
  include/TerminalTab.h
  include/TerminalWidget.h
  include/SshSession.h
  include/ProfileStore.h
  include/ProfileManagerDialog.h
  include/ThemeDialog.h
)

target_include_directories(SimpleSSHTerm PRIVATE include)

if (LIBSSH_FOUND)
  target_include_directories(SimpleSSHTerm PRIVATE ${LIBSSH_INCLUDE_DIRS})
  target_link_directories(SimpleSSHTerm PRIVATE ${LIBSSH_LIBRARY_DIRS})
  target_link_libraries(SimpleSSHTerm PRIVATE ${LIBSSH_LIBRARIES})
  target_compile_definitions(SimpleSSHTerm PRIVATE HAVE_LIBSSH=1)
endif()

if (SODIUM_FOUND)
  target_include_directories(SimpleSSHTerm PRIVATE ${SODIUM_INCLUDE_DIRS})
  target_link_directories(SimpleSSHTerm PRIVATE ${SODIUM_LIBRARY_DIRS})
  target_link_libraries(SimpleSSHTerm PRIVATE ${SODIUM_LIBRARIES})
  target_compile_definitions(SimpleSSHTerm PRIVATE HAVE_SODIUM=1)
endif()

if (LIBVTERM_FOUND)
  target_include_directories(SimpleSSHTerm PRIVATE ${LIBVTERM_INCLUDE_DIRS})
  target_link_directories(SimpleSSHTerm PRIVATE ${LIBVTERM_LIBRARY_DIRS})
  target_link_libraries(SimpleSSHTerm PRIVATE ${LIBVTERM_LIBRARIES})
  target_compile_definitions(SimpleSSHTerm PRIVATE HAVE_LIBVTERM=1)
endif()

target_link_libraries(SimpleSSHTerm PRIVATE Qt5::Widgets)

# Better warnings for dev
if (MSVC)
  target_compile_options(SimpleSSHTerm PRIVATE /W4)
else()
  target_compile_options(SimpleSSHTerm PRIVATE -Wall -Wextra -Wpedantic)
endif()
