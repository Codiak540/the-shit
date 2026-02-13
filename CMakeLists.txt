cmake_minimum_required(VERSION 4)
project(shit VERSION 1.0.2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
        main.cpp
)

# Executable
add_executable(shit ${SOURCES})

# Installation
install(TARGETS shit
        RUNTIME DESTINATION bin
        COMPONENT runtime
)

# Install documentation
install(FILES README.md
        DESTINATION share/doc/${PROJECT_NAME}
        COMPONENT documentation
)

# CPack configuration for creating release tarballs
set(CPACK_PACKAGE_NAME "shit")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A vastly improved version of The Fuck, Written in C++")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
        "/\\.git/"
        "/build/"
        "\\.swp$"
        "\\.orig$"
        "/CMakeLists\\.txt\\.user$"
)

include(CPack)
