project(dynamic-delay)

set(dynamic-delay_HEADERS
	dynamic-delay.h
	easing.h)
set(dynamic-delay_SOURCES
	dynamic-delay.c
	easing.c)

if(WIN32)
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
