CMAKE_MINIMUM_REQUIRED( VERSION 3.0 )

PROJECT( RapaV4L2 )

SET( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake" )

IF( CMAKE_SYSTEM_NAME MATCHES "Linux" )
	
	INCLUDE_DIRECTORIES( include )

	SET	( 	HEADERS
			include/RV4L2MemoryBuffer.h
			include/RV4L2ImageFormat.h
			include/RV4L2Image.h
			include/RV4L2ImageConverter.h
			include/RV4L2ImageReader.h
			include/RV4L2ImageWriter.h
			include/RV4L2CapturedImage.h
			include/RV4L2CaptureSettings.h
			include/RV4L2Device.h
		)
	SET	(	SOURCES
			src/RV4L2MemoryBuffer.cpp
			src/RV4L2ImageFormat.cpp
			src/RV4L2Image.cpp		
			src/RV4L2ImageConverter.cpp		
			src/RV4L2ImageReader.cpp		
			src/RV4L2ImageWriter.cpp		
			src/RV4L2CapturedImage.cpp
			src/RV4L2CaptureSettings.cpp		
			src/RV4L2Device.cpp
		)

	ADD_LIBRARY( ${PROJECT_NAME} STATIC ${HEADERS} ${SOURCES} )

	#
	# Install
	#
	SET( EXTRA_SYSTEM_INCLUDE_DIRS ${DirectInput_INCLUDE_DIR} )
	INSTALL(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Targets
			LIBRARY DESTINATION lib
			ARCHIVE DESTINATION lib
			RUNTIME DESTINATION bin )
			#INCLUDES DESTINATION include )		# If uncommented, the ${PROJECT_NAME} target contains INCLUDE_DIRECTORIES information. Importing the target automatically adds this directory to the INCLUDE_DIRECTORIES.
	SET( TARGET_NAMESPACE Rapa:: )
	INSTALL( FILES ${HEADERS} DESTINATION include COMPONENT Devel )		
	EXPORT( EXPORT ${PROJECT_NAME}Targets FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake" NAMESPACE ${TARGET_NAMESPACE} )
	CONFIGURE_FILE( cmake/${PROJECT_NAME}Config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake" @ONLY )
	SET( ConfigPackageLocation lib/cmake/${PROJECT_NAME} )
	INSTALL(EXPORT ${PROJECT_NAME}Targets
			FILE ${PROJECT_NAME}Targets.cmake
			NAMESPACE ${TARGET_NAMESPACE}
			DESTINATION ${ConfigPackageLocation} )
	INSTALL( FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake" DESTINATION ${ConfigPackageLocation} COMPONENT Devel )

	ADD_SUBDIRECTORY( samples )
ELSE()
	MESSAGE("${PROJECT_NAME} is Linux only")
ENDIF()



