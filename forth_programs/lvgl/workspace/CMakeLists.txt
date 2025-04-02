cmake_minimum_required(VERSION 3.28)

project(lvgl_workspace LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../lvgl/CMakeLists.txt)
include_directories(${CMAKE_CURRENT_LIST_DIR})
include_directories(${CMAKE_CURRENT_LIST_DIR}/..) # lvgl/lvgl.h

add_executable(lvgl_workspace
    main.c
    mouse_cursor_icon.c
    runtime_lvgl.c
    ../../../mcp_forth.c
    ../../../compile.c
    ../../../vm_backend.c
    ../../../vm_engine.c
    ../../../runtime_io.c

    ../../../x86-32_backend_generated.c
    ../../../x86-32_backend.c
    ../../../x86-32_engine_asm.o
    ../../../x86-32_engine.c
)

# 32 bit
set_target_properties(lvgl_workspace PROPERTIES COMPILE_OPTIONS "-m32" LINK_FLAGS "-m32")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -g -Wall -Wextra -fsanitize=address -Wno-unused-parameter -Wno-missing-field-initializers")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -g -Wall -Wextra -fsanitize=address -Wno-unused-parameter -Wno-missing-field-initializers")
set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -m32 -fsanitize=address -Wno-unused-parameter -Wno-missing-field-initializers")

target_link_libraries(lvgl_workspace lvgl)
target_link_libraries(lvgl_workspace pthread)
target_link_libraries(lvgl_workspace SDL2)
target_link_libraries(lvgl_workspace m)
# target_link_libraries(lvgl_workspace lvgl_thorvg)
# target_link_libraries(lvgl_workspace lvgl_examples)
# target_link_libraries(lvgl_workspace lvgl_demos)
# target_link_libraries(lvgl_workspace jpeg)
# target_link_libraries(lvgl_workspace png)

# target_link_libraries(lvgl_workspace GL)
# target_link_libraries(lvgl_workspace GLEW)
# target_link_libraries(lvgl_workspace glfw)
# target_link_libraries(lvgl_workspace assimp)
