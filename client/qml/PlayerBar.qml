import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

// 底部播放条：MediaPlayer 播放服务端给的音源地址。
// 本地歌曲走 /api/songs/{id}/stream（支持 Range）；在线歌曲走酷我 CDN 直链。
// 在线歌曲没有数据库 songId，playingSongId 为 0，但也会写入 play_history（记录 online 信息）。
Rectangle {
    id: bar

    property int playingSongId: 0
    property bool hasMedia: false
    property string currentTitle: ""
    property string currentArtist: ""

    implicitHeight: 110
    color: "#2d2d2d"

    function formatMs(ms) {
        if (ms <= 0)
            return "00:00"
        const total = Math.floor(ms / 1000)
        const m = Math.floor(total / 60)
        const s = total % 60
        return (m < 10 ? "0" + m : "" + m) + ":" + (s < 10 ? "0" + s : "" + s)
    }

    // 切歌前把已播进度写回服务端；不足 3 秒视为误点，不记录
    function flushProgress() {
        if (bar.playingSongId <= 0 && app.currentRid.length === 0)
            return

        const seconds = Math.floor(player.position / 1000)
        if (seconds >= 3)
            app.recordPlay(bar.playingSongId, seconds, false)

        bar.playingSongId = 0
    }

    MediaPlayer {
        id: player

        audioOutput: AudioOutput {
            id: audioOutput
            volume: volumeSlider.value / 100.0
        }

        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.EndOfMedia) {
                const total = Math.round(player.duration / 1000)
                if ((bar.playingSongId > 0 || app.currentRid.length > 0) && total > 0) {
                    app.recordPlay(bar.playingSongId, total, true)
                    bar.playingSongId = 0
                }
            }
        }

        onErrorOccurred: function (err, message) {
            app.showToast(qsTr("播放失败：%1").arg(message))
        }
    }

    Connections {
        target: app

        function onPlayRequested() {
            bar.flushProgress()
            bar.playingSongId = app.currentSongId
            bar.hasMedia = app.currentStreamUrl.length > 0
            bar.currentTitle = app.currentTitle
            bar.currentArtist = app.currentArtist
            player.source = app.currentStreamUrl
            player.play()

            // 播放时自动加载歌词
            app.loadLrc()
        }

        function onCurrentLrcChanged() {
            lrcText.text = app.currentLrc.length > 0
                ? app.currentLrc
                : qsTr("暂无歌词")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 歌词行
        Label {
            id: lrcText
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: "#a3a3a3"
            font.pixelSize: 13
            text: qsTr("暂无歌词")
        }

        // 控制行
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 16

            // 上一首
            Image {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                source: "qrc:/image/prev.png"
                fillMode: Image.PreserveAspectFit
                opacity: bar.hasMedia ? 1.0 : 0.4

                MouseArea {
                    anchors.fill: parent
                    enabled: bar.hasMedia
                    onClicked: app.playPrev()
                }
            }

            // 播放/暂停
            Image {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                source: player.playbackState === MediaPlayer.PlayingState
                        ? "qrc:/image/pause.png" : "qrc:/image/play.png"
                fillMode: Image.PreserveAspectFit
                opacity: bar.hasMedia ? 1.0 : 0.4

                MouseArea {
                    anchors.fill: parent
                    enabled: bar.hasMedia
                    onClicked: {
                        if (player.playbackState === MediaPlayer.PlayingState)
                            player.pause()
                        else
                            player.play()
                    }
                }
            }

            // 下一首
            Image {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                source: "qrc:/image/next.png"
                fillMode: Image.PreserveAspectFit
                opacity: bar.hasMedia ? 1.0 : 0.4

                MouseArea {
                    anchors.fill: parent
                    enabled: bar.hasMedia
                    onClicked: app.playNext()
                }
            }

            // 歌曲信息
            ColumnLayout {
                Layout.preferredWidth: 200
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: bar.currentTitle.length > 0 ? bar.currentTitle : qsTr("未在播放")
                    font.bold: true
                    color: "#ffffff"
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: bar.currentArtist
                    color: "#a3a3a3"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    visible: bar.currentArtist.length > 0
                }
            }

            // 进度条
            Slider {
                id: progress

                Layout.fillWidth: true
                enabled: bar.hasMedia
                from: 0
                to: player.duration > 0 ? player.duration : 1

                Binding {
                    target: progress
                    property: "value"
                    value: player.position
                    when: !progress.pressed
                }

                onMoved: player.setPosition(Math.round(value))
            }

            // 时间
            Label {
                text: bar.formatMs(player.position) + " / " + bar.formatMs(player.duration)
                color: "#d4d4d4"
                font.pixelSize: 12
            }

            // 音量图标：volume.png 静音 / voice_clicked.png 有声
            Image {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                source: volumeSlider.value <= 0 ? "qrc:/image/volume.png"
                                                : "qrc:/image/voice_clicked.png"
                fillMode: Image.PreserveAspectFit

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        volumeSlider.value = volumeSlider.value > 0 ? 0 : 60
                    }
                }
            }

            // 音量滑块
            Slider {
                id: volumeSlider
                Layout.preferredWidth: 80
                from: 0
                to: 100
                value: 60
            }
        }
    }
}
