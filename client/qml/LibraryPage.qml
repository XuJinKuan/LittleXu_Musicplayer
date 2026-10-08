import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    property string keyword: ""

    readonly property bool narrow: width < 600

    background: Rectangle { color: "transparent" }

    function doSearch() {
        page.keyword = searchField.text
        app.loadSongs(page.keyword, 1)
    }

    Component.onCompleted: app.loadSongs("", 1)

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
                color: "#a3a3a3"
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
                required property int index

                width: songList.width
                padding: 10

                // 左击播放，右击删除
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            app.playSong(songDelegate.songId,
                                         songDelegate.title,
                                         songDelegate.artist,
                                         songDelegate.index)
                        } else if (mouse.button === Qt.RightButton) {
                            contextMenu.popup()
                        }
                    }

                    Menu {
                        id: contextMenu

                        MenuItem {
                            text: qsTr("从歌曲库删除")
                            onTriggered: {
                                app.deleteSong(songDelegate.songId)
                            }
                        }
                    }
                }

                background: Rectangle {
                    color: songDelegate.hovered ? "#2d2d2d" : "transparent"
                    radius: 4
                }

                contentItem: RowLayout {
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: songDelegate.title
                            font.bold: true
                            color: "#ffffff"
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            color: "#a3a3a3"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            text: [songDelegate.artist, songDelegate.album]
                                  .filter(function (s) { return s && s.length > 0 })
                                  .join(" · ")
                        }
                    }

                    Label {
                        text: songDelegate.genre
                        color: "#a3a3a3"
                        visible: text.length > 0 && !page.narrow
                    }

                    Label {
                        text: songDelegate.durationText
                        color: "#d4d4d4"
                    }

                    Label {
                        text: qsTr("播放 %1").arg(songDelegate.playCount)
                        color: "#737373"
                        font.pixelSize: 12
                        visible: !page.narrow
                    }

                    Button {
                        text: qsTr("删除")
                        enabled: !app.busy
                        onClicked: app.deleteSong(songDelegate.songId)
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: songList.count === 0 && !app.busy
                text: qsTr("没有可显示的歌曲")
                color: "#737373"
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
                color: "#d4d4d4"
            }

            Button {
                text: qsTr("下一页")
                enabled: app.currentPage * app.songLimit < app.songTotal && !app.busy
                onClicked: app.loadSongs(page.keyword, app.currentPage + 1)
            }
        }
    }
}
