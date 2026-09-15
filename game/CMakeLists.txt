file(GLOB_RECURSE GAME CONFIGURE_DEPENDS "*.cpp")

add_library(game_lib ${GAME})

target_include_directories(game_lib PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(game_lib PUBLIC engine_lib)
