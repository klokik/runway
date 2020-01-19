
set(COMMON_LIBRARIES
  ${WLROOTS_LIBS}
  ${WAYLAND_LIBS}
  GLESv2
  xkbcommon
)

message("Link libs ${COMMON_LIBRARIES}")

####
add_executable(rwc-init src/sandbox/hello-rwc.cc)
target_include_directories(rwc-init PRIVATE ${WLROOTS_INCLUDE})
target_link_libraries(rwc-init ${COMMON_LIBRARIES})

####
add_executable(wlr-simple src/sandbox/simple.cc)
target_include_directories(wlr-simple PRIVATE ${WLROOTS_INCLUDE})
target_link_libraries(wlr-simple ${COMMON_LIBRARIES})

