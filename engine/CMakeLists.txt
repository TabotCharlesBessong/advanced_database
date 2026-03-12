# C++ core engine configuration
add_library(dbcore STATIC
    Database.cpp
    TableCatalog.cpp
    Page.cpp
    FileManager.cpp
    BufferPool.cpp
    RecordLayout.cpp
    SlottedPage.cpp
    TableHeap.cpp
    QueryExecution.cpp
    SqlLexer.cpp
    SqlParser.cpp
    Statistics.cpp
    QueryPlanner.cpp
    BTreeIndex.cpp
    IndexManager.cpp
    TransactionManager.cpp
)

target_include_directories(dbcore PUBLIC ..)

add_executable(dbcore_engine
    EngineMain.cpp
)

target_link_libraries(dbcore_engine
    dbcore
)

# Add test executable
add_executable(dbcore_test
    DatabaseTest.cpp
)

target_link_libraries(dbcore_test
    dbcore
    gtest_main
)

add_test(NAME dbcore_test COMMAND dbcore_test)