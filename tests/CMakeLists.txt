file(GLOB PICAZOO_TEST_SUPPORT CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/support/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/support/*.h")

add_library(picazoo_testsupport STATIC ${PICAZOO_TEST_SUPPORT})
target_include_directories(picazoo_testsupport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(picazoo_testsupport PUBLIC picazoo_core Qt6::Test)
target_compile_definitions(picazoo_testsupport PUBLIC
    PICAZOO_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# Une cible et un test CTest par fichier tst_*.cpp
file(GLOB PICAZOO_TEST_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/tst_*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/tst_*.cpp")

# Les DLL de vcpkg et de Qt doivent être trouvées à l'exécution des tests.
cmake_path(ABSOLUTE_PATH Qt6_DIR NORMALIZE OUTPUT_VARIABLE _qt_cmake_dir)
cmake_path(GET _qt_cmake_dir PARENT_PATH _qt_lib_cmake)
cmake_path(GET _qt_lib_cmake PARENT_PATH _qt_lib)
cmake_path(GET _qt_lib PARENT_PATH _qt_prefix)
set(PICAZOO_TEST_PATH
    "PATH=path_list_prepend:${_qt_prefix}/bin"
    "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin")

foreach(_file IN LISTS PICAZOO_TEST_FILES)
    get_filename_component(_name "${_file}" NAME_WE)
    qt_add_executable(${_name} "${_file}")
    set_target_properties(${_name} PROPERTIES WIN32_EXECUTABLE OFF)
    target_link_libraries(${_name} PRIVATE picazoo_testsupport)
    if(_name STREQUAL "tst_browsing")
        # Les modèles de l'application sont testés directement.
        target_sources(${_name} PRIVATE
            "${PROJECT_SOURCE_DIR}/src/app/models/FolderTreeModel.cpp"
            "${PROJECT_SOURCE_DIR}/src/app/models/GridRowModel.cpp"
            "${PROJECT_SOURCE_DIR}/src/app/models/ItemSelection.cpp"
            "${PROJECT_SOURCE_DIR}/src/app/models/FolderTreeModel.h"
            "${PROJECT_SOURCE_DIR}/src/app/models/GridRowModel.h"
            "${PROJECT_SOURCE_DIR}/src/app/models/ItemSelection.h")
        target_include_directories(${_name} PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
    endif()
    if(_name STREQUAL "tst_showpath")
        # « Ouvrir dans Picazoo » (005) : le contrôleur de l'application est testé directement.
        target_sources(${_name} PRIVATE
            "${PROJECT_SOURCE_DIR}/src/app/controllers/AppController.cpp"
            "${PROJECT_SOURCE_DIR}/src/app/controllers/AppController.h")
        target_include_directories(${_name} PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
    endif()
    add_test(NAME ${_name} COMMAND ${_name})
    set_tests_properties(${_name} PROPERTIES
        ENVIRONMENT_MODIFICATION "${PICAZOO_TEST_PATH}"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
