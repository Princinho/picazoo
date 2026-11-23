# picazoo-viewer.exe : visionneuse autonome (005). Ne lie ni la bibliothèque ni
# SQLite : seuls les décodeurs, le parcours de dossier et le code Windows du cœur
# sont utilisés (le lieur n'embarque que ce qui est référencé).
file(GLOB PICAZOO_VIEWER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB PICAZOO_VIEWER_QML_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml")

qt_add_executable(picazoo_viewer WIN32 ${PICAZOO_VIEWER_SOURCES} picazoo-viewer.rc)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(picazoo_viewer
    URI PicazooViewer
    VERSION 1.0
    QML_FILES ${PICAZOO_VIEWER_QML_FILES})

target_include_directories(picazoo_viewer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(picazoo_viewer PRIVATE picazoo_core Qt6::Quick Qt6::QuickControls2 Qt6::Network d3d11 dxgi dcomp)

set_target_properties(picazoo_viewer PROPERTIES OUTPUT_NAME "picazoo-viewer")

# Paquet (cmake --install) : à côté de picazoo.exe, dans bin/.
install(TARGETS picazoo_viewer RUNTIME DESTINATION bin)
qt_generate_deploy_qml_app_script(
    TARGET picazoo_viewer
    OUTPUT_SCRIPT picazoo_viewer_deploy_script
    NO_TRANSLATIONS)
install(SCRIPT "${picazoo_viewer_deploy_script}")
