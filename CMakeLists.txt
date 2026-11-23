cmake_minimum_required(VERSION 3.25)

# vcpkg : VCPKG_ROOT si défini, sinon le vcpkg livré avec Visual Studio Build Tools 2026.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    else()
        foreach(_vs IN ITEMS
                "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools"
                "C:/Program Files/Microsoft Visual Studio/18/Community"
                "C:/Program Files/Microsoft Visual Studio/18/Professional"
                "C:/Program Files/Microsoft Visual Studio/18/Enterprise")
            if(EXISTS "${_vs}/VC/vcpkg/scripts/buildsystems/vcpkg.cmake")
                set(CMAKE_TOOLCHAIN_FILE "${_vs}/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
                break()
            endif()
        endforeach()
    endif()
endif()

project(Picazoo VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # /external:* : pas d'avertissements pour les en-têtes de Qt et de vcpkg.
    # C4702 (code inaccessible) est émis à l'optimisation pour du code inliné de Qt et
    # ignore /external : on le désactive.
    add_compile_options(/W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus /MP /external:anglebrackets /external:W0 /wd4702)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Test)
qt_standard_project_setup(REQUIRES 6.12)

find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(libheif CONFIG REQUIRED)
find_package(libraw CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)
find_package(TinyEXIF CONFIG REQUIRED)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(tests)
add_subdirectory(bench)
add_subdirectory(tools/make-icons)
add_subdirectory(tools/make-testdata)
