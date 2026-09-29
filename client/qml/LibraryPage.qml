import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    property string keyword: ""

    function doSearch() {
        page.keyword = searchField.text
        app.loadSongs(page.keyword, 1)
    }

    Component.onCompleted: app.loadSongs("", 1)

    Connections {
        target: app

        function onSongDetailReady(detail) {
            detailPopup.openWith(detail)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("按歌曲名、歌手或专辑名搜索")
                selectByMouse: true
                onAccepted: page.doSearch()
            }

            Button {
                text: qsTr("搜索")
                enabled: !app.busy
                onClicked: page.doSearch()
            }

            Button {
                text: qsTr("重置")
                enabled: !app.busy
                onClicked: {
                    searchField.text = ""
                    page.keyword = ""
                    app.loadSongs("", 1)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("共 %1 首，每页 %2 首").arg(app.songTotal).arg(app.songLimit)
                color: "#6b7280"
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

        ListView {
            id: songList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: app.songs
            boundsBehavior: Flickable.StopAtBounds

            delegate: ItemDelegate {
                id: songDelegate

                required property int songId
                required property string title
                required property string artist
                required property string album
                required property string genre
                required property int playCount
                required property string durationText

                width: songList.width
                padding: 10
                onClicked: app.loadSongDetail(songDelegate.songId)

                contentItem: RowLayout {
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: songDelegate.title
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            color: "#6b7280"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            text: [songDelegate.artist, songDelegate.album]
                                  .filter(function (s) { return s && s.length > 0 })
                                  .join(" · ")
                        }
                    }

                    Label {
                        text: songDelegate.genre
                        color: "#6b7280"
                        visible: text.length > 0
                    }

                    Label {
                        text: songDelegate.durationText
                        color: "#374151"
                    }

                    Label {
                        text: qsTr("播放 %1").arg(songDelegate.playCount)
                        color: "#9ca3af"
                        font.pixelSize: 12
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: songList.count === 0 && !app.busy
                text: qsTr("没有可显示的歌曲")
                color: "#9ca3af"
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8

            Button {
                text: qsTr("上一页")
                enabled: app.currentPage > 1 && !app.busy
                onClicked: app.loadSongs(page.keyword, app.currentPage - 1)
            }

            Label {
                text: qsTr("第 %1 页").arg(app.currentPage)
            }

            Button {
                text: qsTr("下一页")
                enabled: app.currentPage * app.songLimit < app.songTotal && !app.busy
                onClicked: app.loadSongs(page.keyword, app.currentPage + 1)
            }
        }
    }

    SongDetailPopup {
        id: detailPopup
    }
}