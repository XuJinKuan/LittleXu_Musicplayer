#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "appcontroller.h"

// 固定深色配色：Fusion 样式默认跟随系统深浅色，这里显式覆盖，
// 保证任何系统下都是深底白字（尤其是 TextField/Button/ComboBox/SpinBox/Menu）。
static QPalette buildFixedDarkPalette()
{
    QPalette palette;

    palette.setColor(QPalette::Window, QColor("#1e1e1e"));
    palette.setColor(QPalette::WindowText, QColor("#ffffff"));
    palette.setColor(QPalette::Base, QColor("#2d2d2d"));
    palette.setColor(QPalette::AlternateBase, QColor("#34352c"));
    palette.setColor(QPalette::ToolTipBase, QColor("#323232"));
    palette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    palette.setColor(QPalette::Text, QColor("#ffffff"));
    palette.setColor(QPalette::Button, QColor("#2d2d2d"));
    palette.setColor(QPalette::ButtonText, QColor("#ffffff"));
    palette.setColor(QPalette::BrightText, QColor("#dc2626"));
    palette.setColor(QPalette::PlaceholderText, QColor("#a3a3a3"));
    palette.setColor(QPalette::Highlight, QColor("#a10b0b"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::Link, QColor("#a3a3a3"));
    palette.setColor(QPalette::LinkVisited, QColor("#a3a3a3"));

    // 禁用态统一弱化为灰色
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#737373"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#737373"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#737373"));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#737373"));

    return palette;
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    // Android 上内置 FFmpeg 后端不支持 https（缺 TLS），强制使用系统原生 MediaPlayer
    qputenv("QT_MEDIA_BACKEND", "android");
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("小徐爱听歌"));
    app.setOrganizationName(QStringLiteral("WUST-DB-Course"));

    // 用内置 Fusion 风格，避免依赖操作系统的原生控件样式
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // 覆盖 Fusion 的默认调色板，避免文字/背景跟随系统深浅色
    app.setPalette(buildFixedDarkPalette());

    // 控制器生命周期与 main 一致，早于 engine 构造以免析构顺序倒置
    AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);

    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}