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
