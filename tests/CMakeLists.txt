# Unit tests. Deliberately link ONLY glm (header-only) and the engine's include
# dir — NOT the `forge` library — so the suite covers pure logic (geometry math,
# ids) and builds fast and headless, with no OpenGL/GLFW context required in CI.
add_executable(forge_tests
    test_main.cpp
    test_geometry.cpp
    test_uuid.cpp)

target_include_directories(forge_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src)
target_link_libraries(forge_tests PRIVATE glm::glm)

add_test(NAME forge_tests COMMAND forge_tests)
