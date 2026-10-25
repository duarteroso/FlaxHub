#include "installrunner.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

#if defined(Q_OS_UNIX)
#include <signal.h>
#include <sys/types.h>
#endif

namespace {

// How long stopStep waits for the step's process to end once it has been killed.
constexpr int stopTimeoutMs = 5000;

// Maps one of git's clone phases, at a percentage of its own, onto the clone step's bar. Receiving
// the objects is most of the download. The last phases check the files out, and for a Git LFS
// repository that is a large download of its own ("Filtering content").
double cloneStepFraction(const QString &phase, int percent)
{
    const double share = percent / 100.0;
    if (phase == QLatin1String("Receiving objects"))
        return 0.8 * share;
    if (phase == QLatin1String("Resolving deltas"))
        return 0.8 + 0.1 * share;

    return 0.9 + 0.1 * share;
}

} // namespace

InstallRunner::InstallRunner(QObject *parent)
    : QObject(parent)
{
    // Quitting the app stops a running step before anything is destroyed. The destructor does the same for a runner
    // that is destroyed earlier.
    if (QCoreApplication *app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, &InstallRunner::stopStep);
}

InstallRunner::~InstallRunner()
{
    stopStep();
}

void InstallRunner::start(const QString &program, const QStringList &arguments, const QString &workingDirectory)
{
    if (m_process) {
        qWarning() << "A step is already running, so" << program << "was not started";
        return;
    }

    // The process is a child of the runner and is deleted once it ends. The pointer is cleared before the end is reported,
    // so the owner can start the next step from inside that report.
    QProcess *process = new QProcess(this);
    m_process = process;
    m_pendingOutput.clear();
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());
#if defined(Q_OS_UNIX)
    // A new session makes the step the leader of its own process group, so stopStep can stop everything it starts.
    process->setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
#endif
    if (!workingDirectory.isEmpty())
        process->setWorkingDirectory(workingDirectory);

    // Without this, git waits on the terminal for credentials when it cannot read the repository.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    process->setProcessEnvironment(environment);

    const QString name = QFileInfo(program).fileName();
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        emitOutput(process->readAllStandardOutput(), false);
    });
    connect(process, &QProcess::finished, this, [this, process, name](int exitCode, QProcess::ExitStatus exitStatus) {
        emitOutput(process->readAllStandardOutput(), true);
        process->deleteLater();
        m_process = nullptr;

        if (exitStatus == QProcess::NormalExit && exitCode == 0)
            emit finished();
        else
            emit failed(QStringLiteral("%1 exited with code %2").arg(name).arg(exitCode));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, name](QProcess::ProcessError error) {
        // Any other error is followed by finished, which reports it.
        if (error != QProcess::FailedToStart)
            return;

        process->deleteLater();
        m_process = nullptr;
        emit failed(QStringLiteral("Could not start %1: ").arg(name) + process->errorString());
    });

    process->start(program, arguments);
}

// Kills the step's process and every process it started. The process's signals are blocked first, so the end of the
// step is not reported to an owner that is being destroyed.
void InstallRunner::stopStep()
{
    if (!m_process)
        return;

    const qint64 pid = m_process->processId();
    m_process->blockSignals(true);
    if (pid > 0) {
#if defined(Q_OS_UNIX)
        // The step leads its own session, so its process group has the step's pid and holds the processes it started.
        killpg(static_cast<pid_t>(pid), SIGKILL);
#elif defined(Q_OS_WIN)
        // taskkill /T ends the process and the children that still point at it as their parent.
        QProcess::execute(QStringLiteral("taskkill"),
                          {QStringLiteral("/T"), QStringLiteral("/F"), QStringLiteral("/PID"), QString::number(pid)});
#endif
    }
    m_process->kill();
    m_process->waitForFinished(stopTimeoutMs);
}

// Reports a step's output as text, then the clone's progress when the output has a progress line. Git redraws its
// progress line, so the last one in the output is the most recent. Only the bytes up to the last line break are
// decoded and matched, so a character or a figure that a read cut in two is whole in the next one. flush reports the
// rest, which is the end of the step's output.
void InstallRunner::emitOutput(const QByteArray &output, bool flush)
{
    m_pendingOutput += output;

    qsizetype end = m_pendingOutput.size();
    if (!flush)
        end = qMax(m_pendingOutput.lastIndexOf('\n'), m_pendingOutput.lastIndexOf('\r')) + 1;
    if (end <= 0)
        return;

    const QString text = QString::fromUtf8(m_pendingOutput.constData(), end);
    m_pendingOutput.remove(0, end);
    emit logReceived(text);

    // The generate script and the build tool report no percentage, so only git's lines match.
    static const QRegularExpression progressPattern(
        QStringLiteral("(Receiving objects|Resolving deltas|Filtering content|Updating files):\\s+(\\d+)%"));
    QRegularExpressionMatchIterator matches = progressPattern.globalMatch(text);
    bool found = false;
    double fraction = 0.0;
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        fraction = cloneStepFraction(match.captured(1), match.captured(2).toInt());
        found = true;
    }

    if (found)
        emit progressChanged(fraction);
}
