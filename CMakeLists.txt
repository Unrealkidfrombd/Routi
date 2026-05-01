cmake_minimum_required(VERSION 3.16)

project(ProductivitySuite
    VERSION 1.0
    LANGUAGES CXX
)

# C++ settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt automatic processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    Charts
    Sql
)

# Recommended Qt setup
qt_standard_project_setup()

# Source files
add_executable(ProductivitySuite
    main.cpp
)

# Link Qt libraries
target_link_libraries(ProductivitySuite PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::Sql
)

# Linux install target
install(TARGETS ProductivitySuite
    RUNTIME DESTINATION bin
)
