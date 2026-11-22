file(GLOB_RECURSE PICAZOO_APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB PICAZOO_QML_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")

qt_add_executable(picazoo WIN32 ${PICAZOO_APP_SOURCES})

qt_add_qml_module(picazoo
    URI Picazoo
    VERSION 1.0
    QML_FILES ${PICAZOO_QML_FILES})

target_include_directories(picazoo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(picazoo PRIVATE picazoo_core Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia dbghelp)

set_target_properties(picazoo PROPERTIES OUTPUT_NAME "picazoo")

# Dossier autonome (T077) : cmake --install build/windows-release --prefix dist/picazoo
# -> dist/picazoo/bin/picazoo.exe
# Qt (QML, plugins, FFmpeg de Qt Multimedia) via le script de déploiement de Qt ;
install(TARGETS picazoo RUNTIME DESTINATION bin)
qt_generate_deploy_qml_app_script(
    TARGET picazoo
    OUTPUT_SCRIPT picazoo_deploy_script
    NO_TRANSLATIONS)
install(SCRIPT "${picazoo_deploy_script}")
# DLL des dépendances vcpkg (libheif, LibRaw, TurboJPEG, SQLite…), copiées directement :
# l'outil « applocal » de vcpkg exige dumpbin, absent hors du Developer PowerShell.
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin/"
            DESTINATION bin
            FILES_MATCHING PATTERN "*.dll")
endif()
