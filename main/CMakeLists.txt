# Get all .cpp source files in this directory
file(GLOB_RECURSE SRCS "*.cpp")

idf_component_register(
    SRCS "${SRCS}"
    INCLUDE_DIRS "."
    PRIV_REQUIRES mldsa44 mldsa87 esp_timer esp_wifi nvs_flash
)

# Use C++17 for TFLite Micro
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler optimizations
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)