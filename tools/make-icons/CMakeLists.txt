# Outil de développement : régénère assets/icons/picazoo.ico et les PNG à partir des SVG.
#   cmake --build --preset windows-release --target make-icons
#   .\build\windows-release\make-icons.exe assets\icons
# Le .ico produit est commité : la compilation de Picazoo n'a pas besoin de cet outil.
find_package(Qt6 6.12 COMPONENTS Svg QUIET)
if(TARGET Qt6::Svg)
    qt_add_executable(make-icons make-icons.cpp)
    set_target_properties(make-icons PROPERTIES WIN32_EXECUTABLE OFF)
    target_link_libraries(make-icons PRIVATE Qt6::Gui Qt6::Svg)
endif()
