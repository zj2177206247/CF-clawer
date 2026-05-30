cmake_minimum_required(VERSION 3.16)
project(CodeforcesViewer C)

set(CMAKE_C_STANDARD 11)

# ---------- curl 配置（third_party 目录下） ----------
set(CURL_ROOT "${CMAKE_SOURCE_DIR}/third_party/curl-8.20.0_2-win64-mingw")
set(CURL_INCLUDE_DIR "${CURL_ROOT}/include")
set(CURL_LIBRARY "${CURL_ROOT}/lib/libcurl.dll.a")

# 打印调试信息，确认路径正确
message(STATUS "CURL_INCLUDE_DIR = ${CURL_INCLUDE_DIR}")
if(NOT EXISTS "${CURL_INCLUDE_DIR}/curl/curl.h")
    message(FATAL_ERROR "找不到 curl/curl.h，请确认路径：${CURL_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${CURL_LIBRARY}")
    message(FATAL_ERROR "找不到 curl 库文件: ${CURL_LIBRARY}")
endif()

# Windows 下 MinGW 需要的附加系统库
set(EXTRA_LIBS ws2_32 wldap32 bcrypt)

# ---------- cJSON 配置（cJSON.c/h 在 src 目录） ----------
add_library(cjson STATIC ${CMAKE_SOURCE_DIR}/src/cJSON.c ${CMAKE_SOURCE_DIR}/src/cJSON.h)
target_include_directories(cjson PUBLIC ${CMAKE_SOURCE_DIR}/src)

# ---------- 主程序 ----------
add_executable(CodeforcesViewer src/main.c)

# 为主程序添加头文件搜索路径（关键修复）
target_include_directories(CodeforcesViewer PRIVATE ${CURL_INCLUDE_DIR})

# 链接库
target_link_libraries(CodeforcesViewer PRIVATE ${CURL_LIBRARY} cjson ${EXTRA_LIBS})

# 构建后自动复制 curl DLL 到可执行文件目录
add_custom_command(TARGET CodeforcesViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CURL_ROOT}/bin/libcurl-x64.dll"
        "$<TARGET_FILE_DIR:CodeforcesViewer>"
    COMMENT "Copying libcurl-x64.dll to output directory"
)