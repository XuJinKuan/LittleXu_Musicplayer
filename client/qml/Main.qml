import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1300
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    visible: true
    title: qsTr("小徐爱听歌")

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: app.loggedIn ? mainPageComponent : loginPageComponent
    }

    Component {
        id: loginPageComponent
        LoginPage {}
    }

    Component {
        id: mainPageComponent
        MainPage {}
    }

    Connections {
        target: app

        function onSessionChanged() {
            const onLoginPage = stack.currentItem !== null
                && stack.currentItem.isLoginPage === true

            if (app.loggedIn && onLoginPage) {
                stack.replace(mainPageComponent)
            } else if (!app.loggedIn && !onLoginPage) {
                stack.replace(loginPageComponent)
            }
        }

        function onToast(message) {
            toastBar.show(message)
        }
    }

    // 轻量提示条：服务端返回的 msg 直接显示在这里
    Rectangle {
        id: toastBar

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28

        width: toastText.implicitWidth + 36
        height: toastText.implicitHeight + 18
        radius: 6
        color: "#323232"
        opacity: 0
        visible: opacity > 0

        function show(message) {
            if (!message || message.length === 0)
                return
            toastText.text = message
            toastBar.opacity = 1
            hideTimer.restart()
        }

        Text {
            id: toastText
            anchors.centerIn: parent
            color: "#ffffff"
            font.pixelSize: 14
        }

        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }

        Timer {
            id: hideTimer
            interval: 2600
            onTriggered: toastBar.opacity = 0
        }
    }
}