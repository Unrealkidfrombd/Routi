cmake_minimum_required(VERSION 3.16)

project(ProductivitySuite
    VERSION 1.0
    LANGUAGES CXX
)

# C++ settings
# Add this near the top of your CMakeLists.txt, after project()
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt automatic processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON) # Automatically processes resources.qrc

# Find Qt6 packages (Added Multimedia)
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    Charts
    Sql
    Multimedia
)

# Recommended Qt setup
qt_standard_project_setup()

# Source files (Added resources.qrc to build target)
add_executable(ProductivitySuite
    main.cpp
    resources.qrc
)

# Link Qt libraries (Added Qt6::Multimedia)
target_link_libraries(ProductivitySuite PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::Sql
    Qt6::Multimedia
)

# Linux install target
install(TARGETS ProductivitySuite
    RUNTIME DESTINATION bin
)
