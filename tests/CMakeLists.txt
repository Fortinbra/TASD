add_executable(test_tasd test_tasd.c)
target_link_libraries(test_tasd PRIVATE tasd)

add_test(NAME test_tasd COMMAND test_tasd)
