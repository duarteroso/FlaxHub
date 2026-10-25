#ifndef INSTALLLAYOUT_H
#define INSTALLLAYOUT_H

#include <QString>

// Where an engine's build puts its files. The install runs the build with these settings and the validator checks
// the same paths, so both read them from here.
namespace InstallLayout {

// The configuration the build step makes. The build arguments and the editor path both use it, so the editor
// the build writes is the one the install checks for.
inline QString buildConfiguration()
{
    return QStringLiteral("Release");
}

// The platform name the build tool takes. It must match the OS the tool runs on.
inline QString buildPlatform()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("Mac");
#else
    return QStringLiteral("Linux");
#endif
}

// The architecture the build step targets. On Windows ARM64 the editor goes in the ARM64 folder (see
// editorDirectoryRelativePath), so the build has to target arm64 too or the install would never find its editor.
inline QString buildArchitecture()
{
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("arm64");
#else
    return QStringLiteral("x64");
#endif
}

// The engine's build tool. Its scripts build it into Binaries/Tools, and Windows names it with the .exe.
inline QString buildToolRelativePath()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Binaries/Tools/Flax.Build.exe");
#else
    return QStringLiteral("Binaries/Tools/Flax.Build");
#endif
}

// Each platform has its own folder under Binaries/Editor, named by the engine's build tool (Platform.cs). The
// editor is in the folder of the configuration that was built.
inline QString editorDirectoryRelativePath()
{
#if defined(Q_OS_WIN)
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("Binaries/Editor/ARM64/%1").arg(buildConfiguration());
#else
    return QStringLiteral("Binaries/Editor/Win64/%1").arg(buildConfiguration());
#endif
#elif defined(Q_OS_MACOS)
    return QStringLiteral("Binaries/Editor/Mac/%1").arg(buildConfiguration());
#else
    return QStringLiteral("Binaries/Editor/Linux/%1").arg(buildConfiguration());
#endif
}

// The editor executable in that folder. Windows names it with the .exe.
inline QString editorRelativePath()
{
#if defined(Q_OS_WIN)
    return editorDirectoryRelativePath() + QStringLiteral("/FlaxEditor.exe");
#else
    return editorDirectoryRelativePath() + QStringLiteral("/FlaxEditor");
#endif
}

} // namespace InstallLayout

#endif
