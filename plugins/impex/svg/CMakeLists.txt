
set(kritasvgimport_SOURCES
    kis_svg_import.cc
    )

kis_add_library(kritasvgimport MODULE ${kritasvgimport_SOURCES})

add_definitions(${SVG_DEFINITIONS})

target_link_libraries(kritasvgimport kritaui )

install(TARGETS kritasvgimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_svg.desktop DESTINATION ${KDE_INSTALL_APPDIR})
