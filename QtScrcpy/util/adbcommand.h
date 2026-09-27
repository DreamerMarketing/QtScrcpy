#ifndef ADBCOMMAND_H
#define ADBCOMMAND_H
#include <QProcess>
#include <QStringList>

inline QStringList adbCommandArguments(QString command)
{
    command = command.trimmed();
    if (command.startsWith("adb ")) command = command.mid(4).trimmed();
    // Keep Android shell quoting intact (including pipes and compound commands).
    if (command.startsWith("shell ")) return {"shell", command.mid(6).trimmed()};
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QProcess::splitCommand(command);
#else
    return command.split(" ", QString::SkipEmptyParts);
#endif
}
#endif
