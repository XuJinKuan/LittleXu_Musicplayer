import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    // 供 Main.qml 判断当前页面类型
    property bool isLoginPage: false
    property int currentNavIndex: 0

    // 窄屏（手机）自适应：小于 600dp 时左侧导航改为顶部横排
    readonly property bool narrow: width < 600

    background: Rectangle {
        color: "#1e1e1e"

        Image {
            anchors.fill: parent
            source: "qrc:/image/background.png"
            fillMode: Image.PreserveAspectCrop
            opacity: 0.3
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶部深灰栏（高64）
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "#323232"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Image {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    source: "qrc:/image/logo.png"
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    visible: !page.narrow
                    text: qsTr("小徐爱听歌")
                    font.pixelSize: 18
                    font.bold: true
                    color: "#ffffff"
                }

                Item { Layout.fillWidth: true }

                Label {
                    elide: Text.ElideRight
                    color: "#a3a3a3"
                    text: app.nickname.length > 0
                          ? qsTr("你好，%1（ID %2）").arg(app.nickname).arg(app.userId)
                          : qsTr("未登录")
                }

                BusyIndicator {
                    running: app.busy
                    visible: app.busy
                    implicitWidth: 24
                    implicitHeight: 24
                }

                Button {
                    text: qsTr("退出登录")
                    onClicked: app.logout()
                }
            }
        }

        // 4px 红色分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 4
            color: "#a10b0b"
        }

        // 窄屏：顶部横排导航（宽屏隐藏）
        Rectangle {
            visible: page.narrow
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#34352c"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: 4

                NavButton {
                    text: qsTr("歌曲库")
                    selected: page.currentNavIndex === 0
                    onClicked: page.currentNavIndex = 0
                }

                NavButton {
                    text: qsTr("在线搜索")
                    selected: page.currentNavIndex === 1
                    onClicked: page.currentNavIndex = 1
                }

                NavButton {
                    text: qsTr("听歌报告")
                    selected: page.currentNavIndex === 2
                    onClicked: page.currentNavIndex = 2
                }
            }
        }

        // 主体区域：左侧导航 + 右侧内容
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 左侧深色导航列（宽200；窄屏隐藏，改用顶部横排导航）
            Rectangle {
                visible: !page.narrow
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: "#34352c"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 12
                    spacing: 4

                    NavButton {
                        text: qsTr("歌曲库")
                        selected: page.currentNavIndex === 0
                        onClicked: page.currentNavIndex = 0
                    }

                    NavButton {
                        text: qsTr("在线搜索")
                        selected: page.currentNavIndex === 1
                        onClicked: page.currentNavIndex = 1
                    }

                    NavButton {
                        text: qsTr("听歌报告")
                        selected: page.currentNavIndex === 2
                        onClicked: page.currentNavIndex = 2
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // 右侧内容区
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: page.currentNavIndex

                LibraryPage {}
                OnlinePage {}
                ReportPage {}
            }
        }

        // 底部播放条
        PlayerBar {
            Layout.fillWidth: true
        }
    }

    // 自定义导航按钮组件
    component NavButton: Item {
        id: navBtn
        property string text: ""
        property bool selected: false

        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 44

        Rectangle {
            anchors.fill: parent
            color: navBtn.selected ? "#a10b0b" : (mouseArea.containsMouse ? "#3d3e35" : "transparent")
            radius: 4
            anchors.leftMargin: 8
            anchors.rightMargin: 8

            Label {
                anchors.centerIn: parent
                text: navBtn.text
                color: navBtn.selected ? "#ffffff" : "#d4d4d4"
                font.pixelSize: 14
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: navBtn.clicked()
            }
        }
    }
}
