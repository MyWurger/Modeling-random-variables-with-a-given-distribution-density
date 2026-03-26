#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QStringList>

namespace
{
void ConfigureQtPluginPaths(int argc, char* argv[])
{
    QString executablePath;
    if (argc > 0 && argv != nullptr && argv[0] != nullptr)
    {
        executablePath = QFileInfo(QString::fromLocal8Bit(argv[0])).absoluteFilePath();
    }

    const QString applicationDir =
        executablePath.isEmpty() ? QDir::currentPath() : QFileInfo(executablePath).absolutePath();

    const QString bundledPluginPath = QDir(applicationDir).absoluteFilePath("../PlugIns");
    QStringList pluginPaths = {bundledPluginPath};

    QStringList validPaths;
    for (const QString& path : pluginPaths)
    {
        if (QDir(path).exists())
        {
            validPaths.append(QDir(path).absolutePath());
        }
    }

    validPaths.removeDuplicates();

    if (!validPaths.isEmpty())
    {
        qputenv("QT_PLUGIN_PATH", validPaths.join(':').toUtf8());

        const QString platformPath =
            QDir(validPaths.front()).absoluteFilePath("platforms");
        if (QDir(platformPath).exists())
        {
            qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", platformPath.toUtf8());
        }

        QCoreApplication::setLibraryPaths(validPaths);
    }
}
} // namespace

int main(int argc, char* argv[])
{
    ConfigureQtPluginPaths(argc, argv);

    QApplication app(argc, argv);
    app.setApplicationName("Моделирование случайных величин");
    app.setOrganizationName("ModelingMethodsLab1");

    QFont uiFont("Avenir Next", 11);
    uiFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(uiFont);

    MainWindow window;
    window.show();

    return app.exec();
}
