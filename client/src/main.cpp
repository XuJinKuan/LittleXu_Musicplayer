#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "appcontroller.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("小徐爱听歌"));
    app.setOrganizationName(QStringLiteral("WUST-DB-Course"));

    // 用内置 Fusion 风格，避免依赖操作系统的原生控件样式
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // 控制器生命周期与 main 一致，早于 engine 构造以免析构顺序倒置
    AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);

    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}