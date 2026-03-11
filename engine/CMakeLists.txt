# C++ core engine configuration
add_library(dbcore STATIC
    Database.cpp
    Page.cpp
    FileManager.cpp
    BufferPool.cpp
)

target_include_directories(dbcore PUBLIC ..)

# Add test executable
add_executable(dbcore_test
    DatabaseTest.cpp
)

target_link_libraries(dbcore_test
    dbcore
    gtest_main
)

add_test(NAME dbcore_test COMMAND dbcore_test)