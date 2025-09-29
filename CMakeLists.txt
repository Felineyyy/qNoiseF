option( PLUGIN_STANDARD_qNoiseF "Install qNoiseF plugin" ON )

if ( PLUGIN_STANDARD_qNoiseF )
	project( qNoiseF )
	 
	AddPlugin( NAME ${PROJECT_NAME} )
		
	add_subdirectory( include )
	add_subdirectory( src )

endif()
