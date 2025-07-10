cmake_minimum_required(VERSION 3.24)
project(RDFPanda_Storage)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-l sqlite3)

include_directories("C:/Program Files/MySQL/MySQL Server 8.0/include")
link_directories("C:/Program Files/MySQL/MySQL Server 8.0/lib")
link_libraries(libmysql)

add_executable(RDFPanda_Storage
        main.cpp
        InputParser.h
        InputParser.cpp
        TripleStore.cpp
        DatalogEngine.cpp
        DatalogEngine.h
        Trie.cpp
        Trie.h
        #BloomFilter.cpp
        #BloomFilter.h
        DatabaseConfig.h

        sqlite3.c
)

# 添加测试目录
add_subdirectory(tests)

#add_subdirectory(tests/googletest)
#include_directories(${PROJECT_SOURCE_DIR}/src/include tests/googletest/include)
#link_directories(${PROJECT_SOURCE_DIR}/lib tests/googletest)
#target_link_libraries(${PROJECT_NAME} gtest)
#set(GOOGLETEST_VERSION 1.16.0)

## 设置 MySQL Connector/C++ 的包含目录
#include_directories("C:/Program Files/MySQL/MySQL Connector C++ 9.3/include/jdbc")
## 设置 MySQL Connector/C++ 的库路径
#link_directories("C:/Program Files/MySQL/MySQL Connector C++ 9.3/lib64")
## 链接 MySQL Connector/C++ 库
#target_link_libraries(RDFPanda_Storage mysqlcppconn-10-vs14)

#include_directories("C:/Program Files/MySQL/MySQL Server 8.0/include")
#link_directories("C:/Program Files/MySQL/MySQL Server 8.0/lib")
#link_libraries(libmysql)
target_link_libraries(RDFPanda_Storage libmysql)

#include_directories("C:\\tools\\SQLite")
#link_directories("C:\\tools\\SQLite")
#target_link_libraries(RDFPanda_Storage sqlite3)