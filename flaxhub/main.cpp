#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QStyleHints>
#include <QTranslator>

int main(int argc, char *argv[])
{
    // Qt's Wayland text-input code warns "Trying to disable ... but 0x0 is focused" when the window
    // receives its first focus before the compositor activates it. It happens with no text fields at all
    // and does not affect input, so hide just that warning.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.qpa.wayland.textinput.warning=false"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("FlaxHub");
    QGuiApplication::setApplicationName("FlaxHub");

    // The Hub is dark only, as Flax is. This also keeps the window frame and native dialogs dark on a light system.
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    // Texts are looked up by id, so the English catalogue is always loaded. Without it the ids would show.
    QTranslator translator;
    if (translator.load(QStringLiteral(":/i18n/flaxhub_en.qm")))
        app.installTranslator(&translator);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("FlaxHub", "Main");

    return QGuiApplication::exec();
}
