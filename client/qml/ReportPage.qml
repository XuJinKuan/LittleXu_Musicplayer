import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    property int selYear: new Date().getFullYear()
    property int selMonth: new Date().getMonth() + 1
    property var report: ({})

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

            Label { text: qsTr("年份") }

            SpinBox {
                id: yearBox
                from: 2000
                to: 2100
                editable: true
                value: page.selYear
                onValueModified: page.selYear = value
            }

            Label { text: qsTr("月份") }

            SpinBox {
                id: monthBox
                from: 1
                to: 12
                editable: true
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
                visible: app.lastError.length > 0
                elide: Text.ElideRight
                Layout.maximumWidth: page.width / 2
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Frame {
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent

                    Label { text: qsTr("本月播放歌曲数"); color: "#6b7280" }
                    Label {
                        text: page.report.songCount !== undefined ? page.report.songCount : "-"
                        font.pixelSize: 28
                        font.bold: true
                        color: "#1f2937"
                    }
                }
            }

            Frame {
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent

                    Label { text: qsTr("本月总时长（分钟）"); color: "#6b7280" }
                    Label {
                        text: page.report.totalMinutes !== undefined ? page.report.totalMinutes : "-"
                        font.pixelSize: 28
                        font.bold: true
                        color: "#1f2937"
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: page.report.items !== undefined && page.report.items.length === 0
            text: qsTr("%1 年 %2 月没有播放记录").arg(page.selYear).arg(page.selMonth)
            color: "#6b7280"
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

                contentItem: RowLayout {
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: reportDelegate.modelData.title
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        text: reportDelegate.modelData.genre
                              && reportDelegate.modelData.genre.length > 0
                              ? reportDelegate.modelData.genre : qsTr("未分类")
                        color: "#6b7280"
                    }

                    Label {
                        text: qsTr("播放 %1 次").arg(reportDelegate.modelData.playTimes)
                    }

                    Label {
                        text: qsTr("%1 秒").arg(reportDelegate.modelData.totalSeconds)
                        color: "#6b7280"
                    }

                    Label {
                        text: reportDelegate.modelData.isFavorite ? qsTr("已收藏") : qsTr("未收藏")
                        color: reportDelegate.modelData.isFavorite ? "#b45309" : "#9ca3af"
                        font.pixelSize: 12
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}