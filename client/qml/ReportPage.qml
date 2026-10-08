import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    property int selYear: new Date().getFullYear()
    property int selMonth: new Date().getMonth() + 1
    property var report: ({})

    // 窄屏（手机）自适应：隐藏次要列，错误信息独占一行
    readonly property bool narrow: width < 600

    background: Rectangle { color: "transparent" }

    function reload() {
        app.loadMonthlyReport(page.selYear, page.selMonth)
    }

    onVisibleChanged: {
        // 首次切到本页时自动查一次，之后由「查询」按钮驱动
        if (visible && page.report.songCount === undefined)
            page.reload()
    }

    Connections {
        target: app

        function onReportReady(data) {
            page.report = data
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label { text: qsTr("年份"); color: "#d4d4d4" }

            SpinBox {
                id: yearBox
                from: 2000
                to: 2100
                editable: true
                locale: Qt.locale("C")
                value: page.selYear
                onValueModified: page.selYear = value
            }

            Label { text: qsTr("月份"); color: "#d4d4d4" }

            SpinBox {
                id: monthBox
                from: 1
                to: 12
                editable: true
                locale: Qt.locale("C")
                value: page.selMonth
                onValueModified: page.selMonth = value
            }

            Button {
                text: qsTr("查询")
                enabled: !app.busy
                onClicked: page.reload()
            }

            Item { Layout.fillWidth: true }

            Label {
                text: app.lastError
                color: "#dc2626"
                visible: !page.narrow && app.lastError.length > 0
                elide: Text.ElideRight
                Layout.maximumWidth: page.width / 2
            }
        }

        // 窄屏：错误信息独占一行（宽屏时在筛选行内）
        Label {
            Layout.fillWidth: true
            text: app.lastError
            color: "#dc2626"
            visible: page.narrow && app.lastError.length > 0
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Frame {
                Layout.fillWidth: true

                background: Rectangle {
                    color: "#2d2d2d"
                    radius: 6
                    border.color: "#404040"
                }

                ColumnLayout {
                    anchors.fill: parent

                    Label { text: qsTr("本月播放歌曲数"); color: "#a3a3a3" }
                    Label {
                        text: page.report.songCount !== undefined ? page.report.songCount : "-"
                        font.pixelSize: 28
                        font.bold: true
                        color: "#e5e5e5"
                    }
                }
            }

            Frame {
                Layout.fillWidth: true

                background: Rectangle {
                    color: "#2d2d2d"
                    radius: 6
                    border.color: "#404040"
                }

                ColumnLayout {
                    anchors.fill: parent

                    Label { text: qsTr("本月总时长（分钟）"); color: "#a3a3a3" }
                    Label {
                        text: page.report.totalMinutes !== undefined ? page.report.totalMinutes : "-"
                        font.pixelSize: 28
                        font.bold: true
                        color: "#e5e5e5"
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: page.report.items !== undefined && page.report.items.length === 0
            text: qsTr("%1 年 %2 月没有播放记录").arg(page.selYear).arg(page.selMonth)
            color: "#a3a3a3"
        }

        ListView {
            id: reportList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            boundsBehavior: Flickable.StopAtBounds
            model: page.report.items !== undefined ? page.report.items : []

            delegate: ItemDelegate {
                id: reportDelegate

                required property var modelData

                width: reportList.width
                padding: 10

                background: Rectangle {
                    color: reportDelegate.hovered ? "#2d2d2d" : "transparent"
                    radius: 4
                }

                contentItem: RowLayout {
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: reportDelegate.modelData.title
                        font.bold: true
                        color: "#ffffff"
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: !page.narrow
                        text: reportDelegate.modelData.genre
                              && reportDelegate.modelData.genre.length > 0
                              ? reportDelegate.modelData.genre : qsTr("未分类")
                        color: "#a3a3a3"
                    }

                    Label {
                        text: qsTr("播放 %1 次").arg(reportDelegate.modelData.playTimes)
                        color: "#d4d4d4"
                    }

                    Label {
                        text: qsTr("%1 秒").arg(reportDelegate.modelData.totalSeconds)
                        color: "#a3a3a3"
                    }

                    Label {
                        visible: !page.narrow
                        text: reportDelegate.modelData.isFavorite ? qsTr("已收藏") : qsTr("未收藏")
                        color: reportDelegate.modelData.isFavorite ? "#b45309" : "#737373"
                        font.pixelSize: 12
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
