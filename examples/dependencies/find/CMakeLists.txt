cmake_minimum_required(VERSION 3.20)
project(httpd LANGUAGES CXX)

find_package(Boost CONFIG REQUIRED)

add_library(httpd src/listener.cpp)
target_include_directories(httpd PUBLIC include)
target_link_libraries(httpd PUBLIC Boost::asio)
