
set(kritatgaexport_SOURCES
    kis_tga_export.cpp
    )

ki18n_wrap_ui(kritatgaexport_SOURCES )

kis_add_library(kritatgaexport MODULE ${kritatgaexport_SOURCES})

target_link_libraries(kritatgaexport kritaui kritaimpex)

install(TARGETS kritatgaexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritatgaimport_SOURCES
    kis_tga_import.cpp
    )

ki18n_wrap_ui(kritatgaimport_SOURCES )

kis_add_library(kritatgaimport MODULE ${kritatgaimport_SOURCES})

target_link_libraries(kritatgaimport kritaui)

install(TARGETS kritatgaimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_tga.desktop DESTINATION ${KDE_INSTALL_APPDIR})
