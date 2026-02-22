include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_executable(typesetting_tests
    test_layout.cpp
    test_css.cpp
    test_style_resolver.cpp
    test_se_integration.cpp
)

target_link_libraries(typesetting_tests
    PRIVATE
        typesetting
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(typesetting_tests)
