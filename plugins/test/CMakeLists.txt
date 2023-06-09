
add_library(test_generator MODULE generator.cpp)
target_link_libraries(test_generator PRIVATE mrdox)
set_target_properties(test_generator PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
