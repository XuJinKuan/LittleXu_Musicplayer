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

    contentItem: ColumnLayout {
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: dialog.detail.title !== undefined ? dialog.detail.title : ""
            font.pixelSize: 20
            font.bold: true
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 18
            rowSpacing: 6

            Label { text: qsTr("歌手"); color: "#6b7280" }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: dialog.detail.artistNames && dialog.detail.artistNames.length > 0
                      ? dialog.detail.artistNames : qsTr("未知")
            }

            Label { text: qsTr("专辑"); color: "#6b7280" }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: dialog.detail.albumName && dialog.detail.albumName.length > 0
                      ? dialog.detail.albumName : qsTr("未知")
            }

            Label { text: qsTr("风格"); color: "#6b7280" }
            Label {
                Layout.fillWidth: true
                text: dialog.detail.genre && dialog.detail.genre.length > 0
                      ? dialog.detail.genre : qsTr("未分类")
            }

            Label { text: qsTr("年份"); color: "#6b7280" }
            Label {
                Layout.fillWidth: true
                text: dialog.detail.year > 0 ? dialog.detail.year : qsTr("未知")
            }

            Label { text: qsTr("时长"); color: "#6b7280" }
            Label {
                Layout.fillWidth: true
                text: dialog.detail.durationText !== undefined ? dialog.detail.durationText : "--:--"
            }

            Label { text: qsTr("码率"); color: "#6b7280" }
            Label {
                Layout.fillWidth: true
                text: dialog.detail.bitrate > 0
                      ? qsTr("%1 kbps").arg(dialog.detail.bitrate) : qsTr("未知")
            }

            Label { text: qsTr("累计播放"); color: "#6b7280" }
            Label {
                Layout.fillWidth: true
                text: dialog.detail.playCount !== undefined ? dialog.detail.playCount : 0
            }

            Label { text: qsTr("文件路径"); color: "#6b7280" }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: dialog.detail.filePath && dialog.detail.filePath.length > 0
                      ? dialog.detail.filePath : qsTr("未登记")
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#9ca3af"
            font.pixelSize: 12
            text: qsTr("当前版本未接入音频播放。可点击下方按钮，向服务端写入一条播放记录以验证 /api/play。")
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("记录一次播放")
            enabled: !app.busy && dialog.detail.songId !== undefined
            onClicked: app.recordPlay(dialog.detail.songId, dialog.detail.duration)
        }
    }
}