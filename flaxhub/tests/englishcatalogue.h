#ifndef ENGLISHCATALOGUE_H
#define ENGLISHCATALOGUE_H

#include <QCoreApplication>
#include <QTranslator>

// The app looks each text up by id in translations/flaxhub_en.ts. tests/CMakeLists.txt compiles that catalogue into
// the model tests as a resource, so they install it and compare the English texts the user sees.
inline bool installEnglishCatalogue(QTranslator &translator)
{
    return translator.load(QStringLiteral(":/i18n/flaxhub_en.qm")) && QCoreApplication::installTranslator(&translator);
}

#endif
