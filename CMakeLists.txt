cmake_minimum_required(VERSION 3.24)
project(RDFPanda_Storage)

set(CMAKE_CXX_STANDARD 17)

# 检测操作系统并设置相应的库路径
if(WIN32)
    # Windows环境（MinGW/MSVC）
    add_compile_options(-l sqlite3)

    include_directories("C:/Program Files/MySQL/MySQL Server 8.0/include")
    link_directories("C:/Program Files/MySQL/MySQL Server 8.0/lib")
    link_libraries(libmysql)

    include_directories("C:/tools/SQLite/src")
    link_directories("C:/tools/SQLite")
    link_libraries(sqlite3)
else()
    # Linux/WSL环境
    # 使用pkg-config或find_package查找MySQL
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(MYSQL QUIET mysqlclient)
        if(MYSQL_FOUND)
            include_directories(${MYSQL_INCLUDE_DIRS})
            link_directories(${MYSQL_LIBRARY_DIRS})
        else()
            # 手动设置常见的MySQL路径
            include_directories(/usr/include/mysql)
            link_directories(/usr/lib/x86_64-linux-gnu)
        endif()
    else()
        # 没有pkg-config，使用默认路径
        include_directories(/usr/include/mysql)
        link_directories(/usr/lib/x86_64-linux-gnu)
    endif()

    # SQLite在Linux下通常在标准路径
    # 不需要显式指定include和lib路径
endif()

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
        LRUCache.cpp
        LRUCache.h
)

# 添加测试目录
#add_subdirectory(tests)

# 链接MySQL库（平台相关）
if(WIN32)
    target_link_libraries(RDFPanda_Storage libmysql)
else()
    target_link_libraries(RDFPanda_Storage mysqlclient)
endif()

target_link_libraries(RDFPanda_Storage sqlite3)