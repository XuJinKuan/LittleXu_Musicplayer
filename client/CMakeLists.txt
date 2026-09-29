cmake_minimum_required(VERSION 3.16)

project(music_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# 客户端第一版：登录 + 歌曲库 + 听歌报告（不含音频播放）
# ---------------------------------------------------------------------------
qt_add_executable(music_client
    src/main.cpp
    src/apiclient.h
    src/apiclient.cpp
    src/songlistmodel.h
    src/songlistmodel.cpp
    src/appcontroller.h
    src/appcontroller.cpp
)

target_include_directories(music_client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(music_client PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

# QML 文件直接打进 qrc，运行期以 qrc:/qml/xxx.qml 加载。
# 不采用 qt_add_qml_module，避免 QML 模块导入路径的一堆约束。
qt_add_resources(music_client "qml"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        qml/Main.qml
        qml/LoginPage.qml
        qml/MainPage.qml
        qml/LibraryPage.qml
        qml/SongDetailPopup.qml
        qml/ReportPage.qml
)

# 第一版保留控制台窗口，便于观察 qDebug 与运行期告警。
# 定稿后可改为 ON 去掉黑框。
set_target_properties(music_client PROPERTIES
    WIN32_EXECUTABLE OFF
)