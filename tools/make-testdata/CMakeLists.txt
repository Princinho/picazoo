# Outil de développement (002) : génère un dossier de photos de test pour valider la
# détection des doublons à la main (specs/002-detection-doublons/quickstart.md).
#   cmake --build --preset windows-release --target picazoo-testdata
#   .\build\windows-release\picazoo-testdata.exe --duplicates C:\PicazooTest\Doublons
qt_add_executable(picazoo-testdata make-testdata.cpp)
set_target_properties(picazoo-testdata PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(picazoo-testdata PRIVATE picazoo_testsupport)
