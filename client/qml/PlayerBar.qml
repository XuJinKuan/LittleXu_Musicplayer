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

    // 窄屏（手机）自适应：隐藏音量控件，歌曲信息单独占一行
    readonly property bool narrow: width < 600

    implicitHeight: narrow ? 96 : 110
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

        // 窄屏：歌曲信息独占一行（宽屏时信息在控制行内）
        Label {
            visible: bar.narrow
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            elide: Text.ElideRight
            color: "#ffffff"
            font.bold: true
            text: bar.currentTitle.length > 0
                  ? bar.currentTitle
                    + (bar.currentArtist.length > 0 ? " · " + bar.currentArtist : "")
                  : qsTr("未在播放")
        }

        // 控制行
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: bar.narrow ? 8 : 16
            Layout.rightMargin: bar.narrow ? 8 : 16
            spacing: bar.narrow ? 8 : 16

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

            // 歌曲信息（窄屏已在上方独立行显示，此处隐藏）
            ColumnLayout {
                visible: !bar.narrow
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

            // 音量图标：volume.png 静音 / voice_clicked.png 有声（窄屏隐藏）
            Image {
                visible: !bar.narrow
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

            // 音量滑块（窄屏隐藏）
            Slider {
                id: volumeSlider
                visible: !bar.narrow
                Layout.preferredWidth: 80
                from: 0
                to: 100
                value: 60
            }
        }
    }
}
