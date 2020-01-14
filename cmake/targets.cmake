
####
add_executable(rwc-init src/sandbox/hello-rwc.cc)
target_include_directories(rwc-init PRIVATE ${WLROOTS_INCLUDE})
target_link_libraries(rwc-init ${WLROOTS_LIBS})


