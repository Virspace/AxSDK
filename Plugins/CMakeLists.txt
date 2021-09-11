#option(BUILD_OSWINDOW "Build the OS Window Plugin" ON)
#if(${BUILD_DASHBOARD})
	add_subdirectory(Window)
	set_target_properties(Window PROPERTIES FOLDER Plugins)
#endif()

add_subdirectory(Win32OpenGL)
set_target_properties(Win32OpenGL PROPERTIES FOLDER Plugins)