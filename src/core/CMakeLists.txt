file(GLOB_RECURSE PICAZOO_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

qt_add_library(picazoo_core STATIC ${PICAZOO_CORE_SOURCES})

qt_add_resources(picazoo_core "picazoo_core_sql"
    PREFIX "/sql"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/index/migrations"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/index/migrations/001_initial.sql"
        "${CMAKE_CURRENT_SOURCE_DIR}/index/migrations/002_doublons.sql")

target_include_directories(picazoo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Version affichée par l'application ; la CI la fixe d'après le tag (ex. 0.2.0) ou le
# commit (ex. dev-1a2b3c4) : cmake --preset windows-release -DPICAZOO_VERSION=0.2.0
set(PICAZOO_VERSION "${PROJECT_VERSION}-dev" CACHE STRING "Version affichée de Picazoo")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/library/Version.cpp"
    PROPERTIES COMPILE_DEFINITIONS "PICAZOO_VERSION_STRING=\"${PICAZOO_VERSION}\"")

# Ressource de version Windows (VERSIONINFO) de picazoo.exe et picazoo-viewer.exe : nom et
# version du produit, contrôlés par SignPath avant la signature (docs/signature.md).
# Version numérique a.b.c.d ; la CI la fixe (ex. 0.2.0.0 pour un tag, 0.1.0.<n° d'exécution>
# pour main) : -DPICAZOO_VERSION_NUMERIC=0.2.0.0
set(PICAZOO_VERSION_NUMERIC "${PROJECT_VERSION}.0" CACHE STRING "Version numérique a.b.c.d des exécutables")
if(NOT PICAZOO_VERSION_NUMERIC MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "PICAZOO_VERSION_NUMERIC doit être de la forme a.b.c.d (reçu : ${PICAZOO_VERSION_NUMERIC})")
endif()
string(REPLACE "." "," PICAZOO_VERSION_COMMAS "${PICAZOO_VERSION_NUMERIC}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/platform/win/picazoo_versioninfo.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/picazoo_versioninfo.rc" @ONLY)
# Les .rc des exécutables l'incluent (#include "picazoo_versioninfo.rc").
target_include_directories(picazoo_core PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")

if(TARGET libheif::heif)
    set(PICAZOO_HEIF_TARGET libheif::heif)
else()
    set(PICAZOO_HEIF_TARGET heif)
endif()

target_link_libraries(picazoo_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
    PRIVATE
        $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>
        ${PICAZOO_HEIF_TARGET}
        libraw::raw_r
        unofficial::sqlite3::sqlite3
        xxHash::xxhash
        TinyEXIF::TinyEXIF
        Qt6::Network
        shell32 ole32 oleaut32 shlwapi uuid)
