add_subdirectory( version )
add_subdirectory( global )
add_subdirectory( koplugin )

if(KRITA_GUI)
	add_subdirectory( widgetutils )
	add_subdirectory( widgets )
endif()

add_subdirectory( store )
add_subdirectory( flake )
add_subdirectory( basicflakes )
add_subdirectory( pigment )
if(KRITA_GUI)
	add_subdirectory( command )
endif()
add_subdirectory( brush )
add_subdirectory( color )
add_subdirectory( image )

if(KRITA_GUI)
	add_subdirectory( ui )
endif()

add_subdirectory( impex )
add_subdirectory( libkis )
add_subdirectory( resources )
add_subdirectory( metadata )

if(KRITA_GUI)
	add_subdirectory( resourcewidgets )
	add_subdirectory( multiarch )
endif()

