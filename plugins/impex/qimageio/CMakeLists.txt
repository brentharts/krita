
set(kritaqimageioexport_SOURCES
    kis_qimageio_export.cpp
    )

ki18n_wrap_ui(kritaqimageioexport_SOURCES kis_wdg_options_qimageio.ui)
ki18n_wrap_ui(kritaqimageioexport_SOURCES )

kis_add_library(kritaqimageioexport MODULE ${kritaqimageioexport_SOURCES})

target_link_libraries(kritaqimageioexport kritaui kritaimpex)

install(TARGETS kritaqimageioexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritaqimageioimport_SOURCES
    kis_qimageio_import.cpp
    )


kis_add_library(kritaqimageioimport MODULE ${kritaqimageioimport_SOURCES})

target_link_libraries(kritaqimageioimport kritaui)

install(TARGETS kritaqimageioimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_qimageio.desktop DESTINATION ${KDE_INSTALL_APPDIR})
