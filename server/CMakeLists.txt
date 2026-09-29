cmake_minimum_required(VERSION 3.16)

project(music_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# MySQL 客户端库定位
# Qt 6.5.3 官方 Windows 包不含 QMYSQL 驱动（plugins/sqldrivers 下只有
# qsqlite / qsqlodbc / qsqlpsql），因此绕开 QSqlDatabase，直连 MySQL C API。
# ---------------------------------------------------------------------------
set(MYSQL_ROOT "C:/Program Files/MySQL/MySQL Server 8.0"
    CACHE PATH "MySQL Server 8.0 安装根目录")

find_path(MYSQL_INCLUDE_DIR
    NAMES mysql.h
    PATHS "${MYSQL_ROOT}/include"
    NO_DEFAULT_PATH)

find_file(MYSQL_DLL
    NAMES libmysql.dll
    PATHS "${MYSQL_ROOT}/lib"
    NO_DEFAULT_PATH)

# libmysql.dll 在运行期还依赖 OpenSSL 1.1 运行库，这两个文件位于 MySQL 的
# bin 目录（不在 lib 目录），缺失时进程会以 0xC0000135 直接退出。
find_file(MYSQL_SSL_DLL
    NAMES libssl-1_1-x64.dll
    PATHS "${MYSQL_ROOT}/bin"
    NO_DEFAULT_PATH)

find_file(MYSQL_CRYPTO_DLL
    NAMES libcrypto-1_1-x64.dll
    PATHS "${MYSQL_ROOT}/bin"
    NO_DEFAULT_PATH)

if(NOT MYSQL_INCLUDE_DIR)
    message(FATAL_ERROR "找不到 mysql.h，请确认 MYSQL_ROOT（当前：${MYSQL_ROOT}）")
endif()
if(NOT MYSQL_DLL)
    message(FATAL_ERROR "找不到 libmysql.dll，请确认 MYSQL_ROOT（当前：${MYSQL_ROOT}）")
endif()

# MinGW 无法可靠链接 MSVC 的 libmysql.lib，改用 gendef + dlltool
# 从 libmysql.dll 直接生成 MinGW 可用的导入库 libmysql.dll.a。
set(MYSQL_GEN_DIR "${CMAKE_BINARY_DIR}/mysql_gen")
set(MYSQL_IMPLIB  "${MYSQL_GEN_DIR}/libmysql.dll.a")

find_program(GENDEF_EXECUTABLE  NAMES gendef)
find_program(DLLTOOL_EXECUTABLE NAMES dlltool)

if(NOT EXISTS "${MYSQL_IMPLIB}")
    if(NOT GENDEF_EXECUTABLE OR NOT DLLTOOL_EXECUTABLE)
        message(FATAL_ERROR
            "需要 gendef 与 dlltool 生成 libmysql 导入库，但未在 PATH 中找到。"
            " 它们随 MinGW-w64 提供，通常位于 D:/work/mingw64/bin。")
    endif()

    file(MAKE_DIRECTORY "${MYSQL_GEN_DIR}")

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MYSQL_DLL}" "${MYSQL_GEN_DIR}/libmysql.dll"
        RESULT_VARIABLE _copy_rc)
    if(NOT _copy_rc EQUAL 0)
        message(FATAL_ERROR "复制 libmysql.dll 到构建目录失败（${_copy_rc}）")
    endif()

    execute_process(
        COMMAND "${GENDEF_EXECUTABLE}" libmysql.dll
        WORKING_DIRECTORY "${MYSQL_GEN_DIR}"
        RESULT_VARIABLE _gendef_rc)
    if(NOT _gendef_rc EQUAL 0)
        message(FATAL_ERROR "gendef 生成 libmysql.def 失败（${_gendef_rc}）")
    endif()

    execute_process(
        COMMAND "${DLLTOOL_EXECUTABLE}" -d libmysql.def -D libmysql.dll
                -l "${MYSQL_IMPLIB}"
        WORKING_DIRECTORY "${MYSQL_GEN_DIR}"
        RESULT_VARIABLE _dlltool_rc)
    if(NOT _dlltool_rc EQUAL 0)
        message(FATAL_ERROR "dlltool 生成导入库失败（${_dlltool_rc}）")
    endif()

    message(STATUS "已生成 MySQL 导入库：${MYSQL_IMPLIB}")
endif()

# ---------------------------------------------------------------------------
# 目标
# ---------------------------------------------------------------------------
qt_add_executable(music_server
    src/main.cpp
    src/util/config.h
    src/util/mysqlpool.h
    src/util/mysqlpool.cpp
    src/util/http.h
    src/util/http.cpp
    src/util/mailer.h
    src/util/mailer.cpp
    src/dao/userdao.h
    src/dao/userdao.cpp
    src/dao/verifydao.h
    src/dao/verifydao.cpp
    src/dao/songdao.h
    src/dao/songdao.cpp
    src/service/userservice.h
    src/service/userservice.cpp
    src/service/serviceresult.h
    src/service/songservice.h
    src/service/songservice.cpp
    src/router/router.h
    src/router/router.cpp
)

target_include_directories(music_server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${MYSQL_INCLUDE_DIR}"
)

target_link_libraries(music_server PRIVATE
    Qt6::Core
    Qt6::Network
    "${MYSQL_IMPLIB}"
)

# 保留控制台窗口，便于直接观察运行日志
set_target_properties(music_server PROPERTIES
    WIN32_EXECUTABLE OFF
)

# 运行期需要 libmysql.dll 与可执行文件同目录
add_custom_command(TARGET music_server POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${MYSQL_DLL}" "$<TARGET_FILE_DIR:music_server>"
    COMMENT "复制 libmysql.dll 到可执行文件目录")

# libmysql.dll 的 OpenSSL 依赖同样必须与可执行文件同目录
foreach(_openssl_dll "${MYSQL_SSL_DLL}" "${MYSQL_CRYPTO_DLL}")
    if(_openssl_dll)
        add_custom_command(TARGET music_server POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_openssl_dll}" "$<TARGET_FILE_DIR:music_server>"
            COMMENT "复制 ${_openssl_dll} 到可执行文件目录")
    else()
        message(WARNING
            "未在 ${MYSQL_ROOT}/bin 找到 OpenSSL 运行库，"
            "启动前请手工复制 libssl-1_1-x64.dll 与 libcrypto-1_1-x64.dll。")
    endif()
endforeach()

# SMTP 授权码等私密配置放在 mail.local.ini（已在 .gitignore 中忽略）。
# 只有文件存在时才复制，否则首次配置前构建会直接报错。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mail.local.ini")
    add_custom_command(TARGET music_server POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/mail.local.ini"
                "$<TARGET_FILE_DIR:music_server>"
        COMMENT "复制 mail.local.ini 到可执行文件目录")
else()
    message(STATUS
        "未找到 server/mail.local.ini：构建产物不会包含 SMTP 授权码，"
        "注册验证码功能需要环境变量 MUSIC_SMTP_AUTH_CODE 或手工放置该文件。")
endif()