import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    // 供 Main.qml 判断当前页面类型
    property bool isLoginPage: false

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                Layout.leftMargin: 12
                text: qsTr("小徐爱听歌")
                font.pixelSize: 18
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: "#6b7280"
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

    footer: TabBar {
        id: tabBar

        TabButton { text: qsTr("歌曲库") }
        TabButton { text: qsTr("听歌报告") }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        LibraryPage {}
        ReportPage {}
    }
}