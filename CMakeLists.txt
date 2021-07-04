cmake_minimum_required(VERSION 3.19)
project(WebCrawler)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS -pthread)

set(CURL_LIBRARY "-lcurl")
find_package(CURL REQUIRED)

add_executable(WebCrawler main.cpp)

include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(WebCrawler ${CURL_LIBRARIES})
