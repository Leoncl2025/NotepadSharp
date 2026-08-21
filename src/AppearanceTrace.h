#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

#include <utility>

namespace AppearanceTrace {

struct State
{
    QMutex mutex;
    quint64 nextCycle = 0;
    quint64 activeCycle = 0;
    QStringList records;
};

inline State &state()
{
    static State value;
    return value;
}

inline QString outputPath()
{
    return qEnvironmentVariable("NOTEPADSHARP_APPEARANCE_TRACE");
}

inline bool enabled()
{
    return !qEnvironmentVariableIsEmpty("NOTEPADSHARP_APPEARANCE_TRACE");
}

inline void initialize()
{
    if (!enabled())
        return;

    const QString path = outputPath();
    QFileInfo(path).dir().mkpath(QStringLiteral("."));
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        file.write(QStringLiteral("%1 pid=%2 trace-enabled version=%3\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
            .arg(QCoreApplication::applicationPid())
            .arg(QCoreApplication::applicationVersion()).toUtf8());
        file.flush();
    }
}

inline quint64 beginCycle(const QString &trigger, const QString &details)
{
    if (!enabled())
        return 0;

    State &trace = state();
    QMutexLocker locker(&trace.mutex);
    trace.activeCycle = ++trace.nextCycle;
    trace.records = {
        QStringLiteral("%1 pid=%2 cycle=%3 begin trigger=%4 %5")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
            .arg(QCoreApplication::applicationPid())
            .arg(trace.activeCycle)
            .arg(trigger, details)
    };
    return trace.activeCycle;
}

inline quint64 currentCycle()
{
    State &trace = state();
    QMutexLocker locker(&trace.mutex);
    return trace.activeCycle;
}

inline void record(quint64 cycle, const QString &component, qint64 elapsedMs,
                   const QString &details = {})
{
    if (cycle == 0)
        return;

    State &trace = state();
    QMutexLocker locker(&trace.mutex);
    if (trace.activeCycle != cycle)
        return;

    trace.records.append(QStringLiteral("cycle=%1 component=%2 elapsed-ms=%3 %4")
        .arg(cycle).arg(component).arg(elapsedMs).arg(details));
}

inline void endCycle(quint64 cycle, qint64 elapsedMs, const QString &details)
{
    if (cycle == 0)
        return;

    QStringList records;
    {
        State &trace = state();
        QMutexLocker locker(&trace.mutex);
        if (trace.activeCycle != cycle)
            return;

        trace.records.append(QStringLiteral("cycle=%1 end elapsed-ms=%2 %3")
            .arg(cycle).arg(elapsedMs).arg(details));
        records = trace.records;
        trace.records.clear();
        trace.activeCycle = 0;
    }

    const QString path = outputPath();
    QFileInfo(path).dir().mkpath(QStringLiteral("."));
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        file.write(records.join(QLatin1Char('\n')).toUtf8());
        file.write("\n");
        file.flush();
    }
}

class Scope
{
public:
    Scope(QString component, QString details = {})
        : cycle(enabled() ? currentCycle() : 0)
        , component(std::move(component))
        , details(std::move(details))
    {
        if (cycle != 0)
            timer.start();
    }

    ~Scope()
    {
        if (cycle != 0)
            record(cycle, component, timer.elapsed(), details);
    }

private:
    quint64 cycle;
    QString component;
    QString details;
    QElapsedTimer timer;
};

} // namespace AppearanceTrace