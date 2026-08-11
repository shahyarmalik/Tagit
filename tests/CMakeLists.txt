add_executable(tagit_tests
    test_audio_metadata.cpp
    test_filename_intelligence.cpp
    test_search_engine.cpp
    test_duplicate_engine.cpp
    test_tag_service_artwork.cpp
    test_review_flow.cpp
    test_database_service.cpp
    test_player_widget.cpp
    ../src/ui/PlayerWidget.cpp
)

target_link_libraries(tagit_tests PRIVATE
    tagit_core
    Qt6::Widgets
    Qt6::Multimedia
    GTest::gtest_main)

target_include_directories(tagit_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src")

target_compile_features(tagit_tests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(tagit_tests)

enable_warnings(tagit_tests)

