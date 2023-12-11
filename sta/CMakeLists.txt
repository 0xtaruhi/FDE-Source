project(sta)

aux_source_directory(. DIR_SRCS)

add_executable(${PROJECT_NAME} ${DIR_SRCS})

target_link_libraries(${PROJECT_NAME} PRIVATE FDE::common boost_timer)

install(TARGETS ${PROJECT_NAME})