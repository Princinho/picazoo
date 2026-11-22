file(GLOB_RECURSE PICAZOO_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

qt_add_library(picazoo_core STATIC ${PICAZOO_CORE_SOURCES})

qt_add_resources(picazoo_core "picazoo_core_sql"
    PREFIX "/sql"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/index/migrations"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/index/migrations/001_initial.sql")

target_include_directories(picazoo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

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
        shell32 ole32 shlwapi)
