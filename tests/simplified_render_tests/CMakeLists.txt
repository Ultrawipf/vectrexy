set(MODULE_NAME simplified_render_tests)

include(${PROJECT_SOURCE_DIR}/cmake/Util.cmake)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

add_executable(${MODULE_NAME} ${SRC_FILES})

# Deliberately not a gtest target: this drives the real Emulator and needs no fixtures, so a plain
# executable returning non-zero on failure keeps it usable from ctest and by hand.
target_link_libraries(${MODULE_NAME}
	PRIVATE
		emulator
		core
)
