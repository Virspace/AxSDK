if(WIN32)
    add_subdirectory(AxAudio)
    set_target_properties(AxAudio PROPERTIES FOLDER Plugins)
endif()

add_subdirectory(AxWindow)
set_target_properties(AxWindow PROPERTIES FOLDER Plugins)

add_subdirectory(AxOpenGL)
set_target_properties(AxOpenGL PROPERTIES FOLDER Plugins)

add_subdirectory(AxScene)
set_target_properties(AxScene PROPERTIES FOLDER Plugins)
add_subdirectory(AxResource)
set_target_properties(AxResource PROPERTIES FOLDER Plugins)