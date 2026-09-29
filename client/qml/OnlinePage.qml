import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 在线搜索：关键词经服务端转发到酷我，点「播放」时由服务端实时解析直链，
// 再交给底部播放条播放。在线歌曲不入库，因此不计入听歌报告。
Page {
    id: page

    background: Rectangle { color: "transparent" }

    function doSearch() {
        app.searchOnline(searchField.text)
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
                placeholderText: qsTr("搜索在线歌曲（歌名或歌手）")
                selectByMouse: true
                onAccepted: page.doSearch()
            }

            Button {
                text: qsTr("搜索")
                enabled: !app.busy
                onClicked: page.doSearch()
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: "#a3a3a3"
                text: qsTr("音源来自酷我；直链实时解析，首次播放可能稍慢。")
            }

            Label {
                text: app.lastError
                color: "#dc2626"
                visible: app.lastError.length > 0
                elide: Text.ElideRight
                Layout.maximumWidth: page.width / 2
            }
        }

        ListView {
            id: onlineList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: app.onlineSongs
            boundsBehavior: Flickable.StopAtBounds

            delegate: ItemDelegate {
                id: onlineDelegate

                required property var modelData

                width: onlineList.width
                padding: 10

                // 左击播放，右击添加到歌曲库
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            app.playOnlineSong(onlineDelegate.modelData.rid,
                                               onlineDelegate.modelData.name,
                                               onlineDelegate.modelData.artist)
                        } else if (mouse.button === Qt.RightButton) {
                            contextMenu.popup()
                        }
                    }

                    Menu {
                        id: contextMenu

                        MenuItem {
                            text: qsTr("添加到歌曲库")
                            onTriggered: {
                                app.addSong(onlineDelegate.modelData.rid,
                                            onlineDelegate.modelData.name,
                                            onlineDelegate.modelData.artist,
                                            onlineDelegate.modelData.album,
                                            onlineDelegate.modelData.durationText)
                            }
                        }
                    }
                }

                background: Rectangle {
                    color: onlineDelegate.hovered ? "#2d2d2d" : "transparent"
                    radius: 4
                }

                contentItem: RowLayout {
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: onlineDelegate.modelData.name
                            font.bold: true
                            color: "#ffffff"
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            color: "#a3a3a3"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            text: [onlineDelegate.modelData.artist, onlineDelegate.modelData.album]
                                  .filter(function (s) { return s && s.length > 0 })
                                  .join(" · ")
                        }
                    }

                    Label {
                        text: onlineDelegate.modelData.durationText
                        color: "#d4d4d4"
                    }

                    Button {
                        text: qsTr("播放")
                        enabled: !app.busy
                        onClicked: app.playOnlineSong(onlineDelegate.modelData.rid,
                                                      onlineDelegate.modelData.name,
                                                      onlineDelegate.modelData.artist)
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: onlineList.count === 0 && !app.busy
                text: qsTr("输入关键词后回车，从在线音源搜索")
                color: "#737373"
            }
        }
    }
}
