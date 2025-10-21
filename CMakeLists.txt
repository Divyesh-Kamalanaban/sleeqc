# This is C:/Users/divyesh/Development/PQC/dilithium2/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Tell ESP-IDF where your extra components are
set(EXTRA_COMPONENT_DIRS components) 

# This is the ONLY place this line should be
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(SleeQC)