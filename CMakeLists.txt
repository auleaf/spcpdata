cmake_minimum_required(VERSION 3.10)
project(httpserver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_MYSQL "Enable MySQL support (optional)" ON)
option(USE_HIREDIS "Enable hiredis (Redis) support (optional)" OFF)

# 用户可配置的MySQL安装路径
set(MYSQL_ROOT_DIR "/usr/local/Cellar/mysql-client/9.5.0" CACHE PATH "Path to MySQL installation directory")

include_directories(${CMAKE_SOURCE_DIR}/external/httplib)
include_directories(${CMAKE_SOURCE_DIR}/external/json)

if(USE_MYSQL)
  # 重置MySQL相关变量
  set(MySQL_FOUND FALSE)
  set(MySQL_INCLUDE_DIR "/usr/local/Cellar/mysql-client/9.5.0/include/mysql")
  set(MySQL_LIBRARY "/usr/local/Cellar/mysql-client/9.5.0/lib/libmysqlclient.dylib")
  
  # 尝试常见的MySQL安装路径和用户指定路径
  set(POTENTIAL_MYSQL_PATHS
    ${MYSQL_ROOT_DIR}/include
    /usr/local/include/mysql
    /usr/local/mysql/include
    /usr/local/opt/mysql-client/include
  )
  
  # 尝试查找mysql.h文件
  find_path(MySQL_INCLUDE_DIR mysql/mysql.h PATHS ${POTENTIAL_MYSQL_PATHS} NO_DEFAULT_PATH)
  
  # 如果在指定路径中未找到，再尝试系统默认路径
  if(NOT MySQL_INCLUDE_DIR)
    find_path(MySQL_INCLUDE_DIR mysql/mysql.h)
  endif()
  
  # 尝试查找MySQL库
  find_library(MySQL_LIBRARY NAMES mysqlclient libmysqlclient PATHS 
    ${MYSQL_ROOT_DIR}/lib
    ${MYSQL_ROOT_DIR}/lib64
    /usr/lib/mysql
    /usr/lib64/mysql
    /usr/local/lib/mysql
    /usr/local/opt/mysql-client/lib
    NO_DEFAULT_PATH
  )
  
  # 如果在指定路径中未找到，再尝试系统默认路径
  if(NOT MySQL_LIBRARY)
    find_library(MySQL_LIBRARY NAMES mysqlclient libmysqlclient)
  endif()
  
  # 检查是否找到了头文件和库
  if(MySQL_INCLUDE_DIR AND MySQL_LIBRARY)
    set(MySQL_FOUND TRUE)
    message(STATUS "MySQL found: ${MySQL_INCLUDE_DIR}")
    message(STATUS "MySQL library found: ${MySQL_LIBRARY}")
    include_directories(${MySQL_INCLUDE_DIR})
    set(HAVE_MYSQL TRUE)
  else()
    message(STATUS "MySQL NOT found; MySQL support will be disabled")
    message(STATUS "Tip: You can specify the MySQL installation path using -DMYSQL_ROOT_DIR=/path/to/mysql when running cmake")
    
    # 显示查找的详细信息
    if(NOT "${MYSQL_ROOT_DIR}" STREQUAL "")
      message(STATUS "Searched for MySQL in: ${MYSQL_ROOT_DIR}/include and ${MYSQL_ROOT_DIR}/lib")
      message(STATUS "Make sure the path contains the MySQL development files.")
    endif()
    
    # 显示每个搜索路径的详细信息
    message(STATUS "Searched include paths:")
    foreach(path ${POTENTIAL_MYSQL_PATHS})
      message(STATUS "  - ${path}")
    endforeach()
    
    # 提供安装MySQL开发库的建议
    if(APPLE)
      message(STATUS "On macOS, you can install MySQL with Homebrew: brew install mysql-connector-c")
    elseif(UNIX AND NOT APPLE)
      message(STATUS "On Linux, you may need to install: apt-get install libmysqlclient-dev (Debian/Ubuntu) or yum install mysql-devel (CentOS/RHEL)")
    endif()
    
    # 如果只找到了部分组件，提供更具体的信息
    if(MySQL_INCLUDE_DIR)
      message(STATUS "Note: Found MySQL include directory, but missing library.")
    elseif(MySQL_LIBRARY)
      message(STATUS "Note: Found MySQL library, but missing include directory.")
    endif()
  endif()
endif()

if(USE_HIREDIS)
  # 首先尝试使用本地的hiredis库
  set(LOCAL_HIREDIS_DIR "${CMAKE_SOURCE_DIR}/external/redis")
  
  # 尝试查找本地hiredis头文件
  find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h PATHS ${LOCAL_HIREDIS_DIR} NO_DEFAULT_PATH)
  
  # 如果本地未找到，再尝试系统路径
  if(NOT HIREDIS_INCLUDE_DIR)
    find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
  endif()
  
  # 尝试查找本地hiredis库（如果已经编译）
  find_library(HIREDIS_LIBRARY hiredis PATHS ${LOCAL_HIREDIS_DIR} NO_DEFAULT_PATH)
  
  # 如果本地未找到，再尝试系统路径
  if(NOT HIREDIS_LIBRARY)
    find_library(HIREDIS_LIBRARY hiredis)
  endif()
  
  if(HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY)
    message(STATUS "hiredis found: ${HIREDIS_INCLUDE_DIR}")
    message(STATUS "hiredis library found: ${HIREDIS_LIBRARY}")
    include_directories(${HIREDIS_INCLUDE_DIR})
    set(HAVE_HIREDIS TRUE)
  else()
    # 即使没有找到预编译的库，我们也可以将hiredis源码添加到项目中
    if(EXISTS "${LOCAL_HIREDIS_DIR}/hiredis.c" AND EXISTS "${LOCAL_HIREDIS_DIR}/hiredis.h")
      message(STATUS "Found hiredis source code, will build it from source")
      include_directories(${LOCAL_HIREDIS_DIR})
      set(HAVE_HIREDIS TRUE)
      set(HIREDIS_INCLUDE_DIR ${LOCAL_HIREDIS_DIR})
    else()
      message(STATUS "hiredis NOT found; Redis support will be disabled")
    endif()
  endif()
endif()

add_executable(httpserver src/main.cpp src/mysql_wrapper.cpp)

target_include_directories(httpserver PRIVATE ${CMAKE_SOURCE_DIR}/external/httplib ${CMAKE_SOURCE_DIR}/external/json)

if(HAVE_MYSQL)
  target_compile_definitions(httpserver PRIVATE HAVE_MYSQL=1)
  # 确保设置MySQL_LIBRARIES变量
  set(MySQL_LIBRARIES ${MySQL_LIBRARY})
  
  if(TARGET MySQL::MySQL)
    target_link_libraries(httpserver PRIVATE MySQL::MySQL)
  else()
    target_link_libraries(httpserver PRIVATE ${MySQL_LIBRARY})
  endif()
  
  # 添加MySQL相关的编译定义
  target_compile_definitions(httpserver PRIVATE HAVE_MYSQL)
endif()

if(HAVE_HIREDIS)
  target_compile_definitions(httpserver PRIVATE HAVE_HIREDIS=1)
  
  # 如果找到预编译库，直接链接
  if(HIREDIS_LIBRARY)
    target_link_libraries(httpserver PRIVATE ${HIREDIS_LIBRARY})
  else()
    # 否则从源码构建hiredis
    message(STATUS "Building hiredis from source")
    
    # 添加hiredis源码文件
    file(GLOB HIREDIS_SOURCES ${LOCAL_HIREDIS_DIR}/*.c)
    
    # 排除可能不需要的文件
    list(REMOVE_ITEM HIREDIS_SOURCES 
      ${LOCAL_HIREDIS_DIR}/test.c 
      ${LOCAL_HIREDIS_DIR}/example.c
    )
    
    # 将hiredis源码添加到项目中
    target_sources(httpserver PRIVATE ${HIREDIS_SOURCES})
  endif()
endif()
