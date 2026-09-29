import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog

    property var detail: ({})

    function openWith(data) {
        dialog.detail = data
        dialog.open()
    }

    title: qsTr("歌曲详情")
    modal: true
    standardButtons: Dialog.Close
    width: 480
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    background: Rectangle {
        color: "#2d2d2d"
        radius: 8
        border.color: "#404040"
    }

    contentItem: ColumnLayout {
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: dialog.detail.title !== undefined ? dialog.detail.title : ""
            font.pixelSize: 20
            font.bold: true
            color: "#ffffff"
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 18
            rowSpacing: 6

            Label { text: qsTr("歌手"); color: "#a3a3a3" }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#d4d4d4"
                text: dialog.detail.artistNames && dialog.detail.artistNames.length > 0
                      ? dialog.detail.artistNames : qsTr("未知")
            }

            Label { text: qsTr("专辑"); color: "#a3a3a3" }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#d4d4d4"
                text: dialog.detail.albumName && dialog.detail.albumName.length > 0
                      ? dialog.detail.albumName : qsTr("未知")
            }

            Label { text: qsTr("风格"); color: "#a3a3a3" }
            Label {
                Layout.fillWidth: true
                color: "#d4d4d4"
                text: dialog.detail.genre && dialog.detail.genre.length > 0
                      ? dialog.detail.genre : qsTr("未分类")
            }

            Label { text: qsTr("年份"); color: "#a3a3a3" }
            Label {
                Layout.fillWidth: true
                color: "#d4d4d4"
                text: dialog.detail.year > 0 ? dialog.detail.year : qsTr("未知")
            }

            Label { text: qsTr("时长"); color: "#a3a3a3" }
            Label {
                Layout.fillWidth: true
                color: "#d4d4d4"
                text: dialog.detail.durationText !== undefined ? dialog.detail.durationText : "--:--"
            }

            Label { text: qsTr("码率"); color: "#a3a3a3" }
            Label {
                Layout.fillWidth: true
                color: "#d4d4d4"
                text: dialog.detail.bitrate > 0
                      ? qsTr("%1 kbps").arg(dialog.detail.bitrate) : qsTr("未知")
            }

            Label { text: qsTr("累计播放"); color: "#a3a3a3" }
            Label {
                Layout.fillWidth: true
                color: "#d4d4d4"
                text: dialog.detail.playCount !== undefined ? dialog.detail.playCount : 0
            }

            Label { text: qsTr("文件路径"); color: "#a3a3a3" }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                color: "#d4d4d4"
                text: dialog.detail.filePath && dialog.detail.filePath.length > 0
                      ? dialog.detail.filePath : qsTr("未登记")
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#737373"
            font.pixelSize: 12
            text: qsTr("点击「播放」将在底部播放条中播放该歌曲（音频由服务端 /api/songs/{id}/stream 提供）。")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                Layout.fillWidth: true
                text: qsTr("播放")
                enabled: dialog.detail.songId !== undefined
                onClicked: {
                    app.playSong(dialog.detail.songId,
                                 dialog.detail.title,
                                 dialog.detail.artistNames)
                    dialog.close()
                }
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("记录一次播放")
                enabled: !app.busy && dialog.detail.songId !== undefined
                onClicked: app.recordPlay(dialog.detail.songId, dialog.detail.duration, true)
            }
        }
    }
}
