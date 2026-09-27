// A subprocess test double; it never connects to or modifies an Android device.
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QThread>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    QString serial;
    if (args.value(0) == "-s") { serial = args.value(1); args = args.mid(2); }
    if (args.value(0) == "devices") { std::puts("List of devices attached\n"); return 0; }
    auto log = [&](const QString &event) {
        QFile file(qEnvironmentVariable("QTSCRCPY_STUB_LOG"));
        if (!file.fileName().isEmpty() && file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QJsonObject entry{{"serial", serial}, {"event", event},
                {"args", QJsonArray::fromStringList(args)}, {"time", QDateTime::currentMSecsSinceEpoch()}};
            file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n');
        }
    };
    log("start");
    QThread::msleep(args.contains("long-running") ? 10000 : 350);
    const QByteArray output = QJsonDocument(QJsonArray::fromStringList(args)).toJson(QJsonDocument::Compact);
    std::puts(output.constData());
    log("end");
    if (serial == "fail") { std::fputs("test device disconnected\n", stderr); return 1; }
    return 0;
}
