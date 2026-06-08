# Minimal Boost CMake config stub used by the documentation example.
#
# A real project would consume the BoostConfig.cmake that Boost ships
# (or the one CMake's FindBoost module generates). Here we declare just
# enough to satisfy `find_package(Boost CONFIG REQUIRED)` and expose the
# `Boost::asio` target backed by the bundled stub headers.

set(Boost_FOUND TRUE)
set(Boost_VERSION "1.83.0")

get_filename_component(_boost_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT TARGET Boost::asio)
    add_library(Boost::asio INTERFACE IMPORTED)
    target_include_directories(Boost::asio INTERFACE "${_boost_root}/include")
endif()
