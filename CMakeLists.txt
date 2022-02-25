project(dynamic-delay VERSION 0.1.3)
set(PROJECT_FULL_NAME "Dynamic Delay")

set(dynamic-delay_HEADERS
	dynamic-delay.h
	easing.h)
set(dynamic-delay_SOURCES
	dynamic-delay.c
	easing.c)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in dynamic-delay.rc)
	list(APPEND dynamic-delay_SOURCES
		dynamic-delay.rc)
endif()

add_library(dynamic-delay MODULE
	${dynamic-delay_HEADERS}
	${dynamic-delay_SOURCES})
target_link_libraries(dynamic-delay
	libobs)

set_target_properties(dynamic-delay PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(dynamic-delay data)
