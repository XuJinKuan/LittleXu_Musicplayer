import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    // 供 Main.qml 判断当前页面类型
    property bool isLoginPage: true
    property bool registerMode: false

    // 验证码重发倒计时（秒）。仅在服务端确认发信成功后才开始。
    property int countdown: 0

    background: Rectangle {
        color: "#1e1e1e"

        Image {
            anchors.fill: parent
            source: "qrc:/image/background.png"
            fillMode: Image.PreserveAspectCrop
            opacity: 0.3
        }
    }

    function doLogin() {
        app.login(userField.text, passField.text)
    }

    function doRegister() {
        app.registerUser(userField.text, passField.text, nickField.text,
                         emailField.text, codeField.text)
    }

    function doSendCode() {
        app.sendEmailCode(emailField.text)
    }

    Connections {
        target: app

        function onEmailCodeSent() {
            page.countdown = 60
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: page.countdown > 0
        onTriggered: page.countdown = Math.max(0, page.countdown - 1)
    }

    // 注册模式比登录模式多三行控件，窗口压到最小高度时内容会超出屏幕，
    // 因此用 Flickable 兜底：内容不高时垂直居中，超出时可滚动。
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: formColumn.implicitHeight + 40
        clip: true

        ColumnLayout {
            id: formColumn
            x: (parent.width - width) / 2
            y: Math.max(20, (parent.height - implicitHeight) / 2)
            width: Math.min(420, page.width - 80)
            spacing: 14

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("小徐爱听歌")
                font.pixelSize: 30
                font.bold: true
                color: "#ffffff"
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: page.registerMode ? qsTr("注册新账号") : qsTr("个人音乐库 · 登录")
                color: "#a3a3a3"
            }

            Frame {
                Layout.fillWidth: true

                background: Rectangle {
                    color: "#66202020"
                    radius: 6
                    border.color: "#66404040"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label { text: qsTr("用户名"); color: "#d4d4d4" }
                    TextField {
                        id: userField
                        Layout.fillWidth: true
                        placeholderText: qsTr("3~50 个字符")
                        selectByMouse: true

                        background: Rectangle {
                            radius: 4
                            color: "#40000000"
                            border.color: "#59ffffff"
                        }
                    }

                    Label { text: qsTr("密码"); color: "#d4d4d4" }
                    TextField {
                        id: passField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: qsTr("至少 6 位")
                        selectByMouse: true
                        onAccepted: page.registerMode ? page.doRegister() : page.doLogin()

                        background: Rectangle {
                            radius: 4
                            color: "#40000000"
                            border.color: "#59ffffff"
                        }
                    }

                    Label {
                        visible: page.registerMode
                        text: qsTr("昵称（可留空，默认与用户名相同）")
                        color: "#d4d4d4"
                    }
                    TextField {
                        id: nickField
                        visible: page.registerMode
                        Layout.fillWidth: true
                        placeholderText: qsTr("例如：徐同学")
                        selectByMouse: true

                        background: Rectangle {
                            radius: 4
                            color: "#40000000"
                            border.color: "#59ffffff"
                        }
                    }

                    Label {
                        visible: page.registerMode
                        text: qsTr("邮箱（用于接收验证码）")
                        color: "#d4d4d4"
                    }
                    TextField {
                        id: emailField
                        visible: page.registerMode
                        Layout.fillWidth: true
                        placeholderText: qsTr("例如：363161953@qq.com")
                        inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                        selectByMouse: true

                        background: Rectangle {
                            radius: 4
                            color: "#40000000"
                            border.color: "#59ffffff"
                        }
                    }

                    Label {
                        visible: page.registerMode
                        text: qsTr("邮箱验证码")
                        color: "#d4d4d4"
                    }
                    RowLayout {
                        visible: page.registerMode
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            id: codeField
                            Layout.fillWidth: true
                            placeholderText: qsTr("4 位数字")
                            maximumLength: 4
                            inputMethodHints: Qt.ImhDigitsOnly
                            selectByMouse: true

                            background: Rectangle {
                                radius: 4
                                color: "#40000000"
                                border.color: "#59ffffff"
                            }
                        }

                        Button {
                            text: page.countdown > 0
                                  ? qsTr("%1 秒后重发").arg(page.countdown)
                                  : qsTr("获取验证码")
                            enabled: page.countdown === 0 && !app.busy
                            onClicked: page.doSendCode()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        spacing: 8

                        Button {
                            Layout.fillWidth: true
                            visible: !page.registerMode
                            text: qsTr("登录")
                            enabled: !app.busy
                            onClicked: page.doLogin()
                        }

                        Button {
                            Layout.fillWidth: true
                            visible: page.registerMode
                            text: qsTr("注册并登录")
                            enabled: !app.busy
                            onClicked: page.doRegister()
                        }

                        Button {
                            Layout.fillWidth: true
                            flat: true
                            text: page.registerMode ? qsTr("返回登录") : qsTr("注册新账号")
                            onClicked: page.registerMode = !page.registerMode
                        }
                    }

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        running: app.busy
                        visible: app.busy
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: app.lastError.length > 0
                        text: app.lastError
                        color: "#dc2626"
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("测试账号：xiaoxu / 123456")
                color: "#737373"
                font.pixelSize: 12
            }
        }
    }
}
