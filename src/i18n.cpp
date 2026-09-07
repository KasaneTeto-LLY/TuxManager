/*
 * Tux Manager - Linux system monitor
 * Copyright (C) 2026 Petr Bena <petr@bena.rocks>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "i18n.h"
#include "configuration.h"
#include "logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QProcess>
#include <QTimer>
#include <QTranslator>

namespace
{
    // Translators live for the whole application lifetime (until restart).
    QTranslator *appTranslator()
    {
        static QTranslator *t = new QTranslator();
        return t;
    }

    QTranslator *qtTranslator()
    {
        static QTranslator *t = new QTranslator();
        return t;
    }

    // Directories that may contain our "<catalog>_<locale>.qm" files.
    // The resource path is always available because translations are embedded
    // at build time (:/i18n/), the filesystem paths cover packaged installs
    // and development builds. A custom directory can be supplied through the
    // TUX_MANAGER_I18N_DIR environment variable.
    QStringList translationSearchDirs()
    {
        QStringList dirs;
        const QString overrideDir = qEnvironmentVariable("TUX_MANAGER_I18N_DIR");
        if (!overrideDir.isEmpty())
            dirs << overrideDir;
        dirs << QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
        dirs << QStringLiteral("/usr/share/tux-manager/translations");
        dirs << QCoreApplication::applicationDirPath() + QStringLiteral("/../share/tux-manager/translations");
        dirs << QStringLiteral(":/i18n");
        return dirs;
    }

    bool isEnglishLanguage(const QString &code)
    {
        if (code.isEmpty())
            return true;
        // "C" / "POSIX" mean the classic unlocalized environment.
        const QString base = code.section('_', 0, 0).toLower();
        return base == QStringLiteral("en")
               || base == QStringLiteral("c")
               || base == QStringLiteral("posix");
    }

    // Try to load a catalog (e.g. "tux-manager" or "qtbase") for the given
    // language. Exact match is preferred, then the language part only.
    bool loadCatalog(QTranslator *translator, const QString &catalog, const QString &language)
    {
        QStringList candidates;
        candidates << language;
        const int underscore = language.indexOf(QLatin1Char('_'));
        if (underscore > 0)
            candidates << language.left(underscore);

        for (const QString &dir : translationSearchDirs())
        {
            for (const QString &lang : candidates)
            {
                const QString fileName = catalog + QLatin1Char('_') + lang + QStringLiteral(".qm");
                const QString path = dir.endsWith(QLatin1Char('/')) ? dir + fileName : dir + QLatin1Char('/') + fileName;
                if (!QFile::exists(path))
                    continue;
                if (translator->load(fileName, dir))
                {
                    LOG_DEBUG(QString("I18n: loaded %1 from %2").arg(fileName, dir));
                    return true;
                }
            }
        }
        return false;
    }

    QString resolveLanguageCode()
    {
        QString code = CFG->Language.trimmed();
        if (code.isEmpty())
            code = QLocale::system().name();
        return code;
    }
}

QStringList I18n::availableLanguageCodes()
{
    // "" = follow system language; these are the shipped languages.
    return { QString(), QStringLiteral("en"), QStringLiteral("zh_CN") };
}

QString I18n::configuredLanguage()
{
    return CFG->Language;
}

QString I18n::installTranslators()
{
    QString code = resolveLanguageCode();

    // Always drop any previously installed translator first so a restart or
    // repeated call can never stack two translations.
    if (appTranslator()->isEmpty() == false)
        QCoreApplication::removeTranslator(appTranslator());
    if (qtTranslator()->isEmpty() == false)
        QCoreApplication::removeTranslator(qtTranslator());

    if (isEnglishLanguage(code))
    {
        LOG_DEBUG("I18n: no translation needed, using built-in English UI");
        return QStringLiteral("en");
    }

    const QString matched = loadCatalog(appTranslator(), QStringLiteral("tux-manager"), code) ? code : QString();
    if (matched.isEmpty())
    {
        // No translation file for this language, fall back to English UI.
        LOG_INFO(QString("I18n: no translation available for '%1', falling back to English").arg(code));
        return QStringLiteral("en");
    }

    // Best effort: also localize Qt built-in dialogs (QMessageBox buttons etc.)
    // using the matching qtbase catalog from the Qt installation.
    const QString qtTranslationsDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (QFile::exists(qtTranslationsDir))
    {
        QStringList candidates;
        candidates << matched;
        const int underscore = matched.indexOf(QLatin1Char('_'));
        if (underscore > 0)
            candidates << matched.left(underscore);

        for (const QString &lang : candidates)
        {
            const QString fileName = QStringLiteral("qtbase_") + lang + QStringLiteral(".qm");
            const QString path = qtTranslationsDir.endsWith(QLatin1Char('/')) ? qtTranslationsDir + fileName
                                                                             : qtTranslationsDir + QLatin1Char('/') + fileName;
            if (QFile::exists(path))
            {
                if (qtTranslator()->load(fileName, qtTranslationsDir))
                {
                    QCoreApplication::installTranslator(qtTranslator());
                    LOG_DEBUG(QString("I18n: loaded Qt base translation %1").arg(fileName));
                }
                break;
            }
        }
    }

    QCoreApplication::installTranslator(appTranslator());
    LOG_INFO(QString("I18n: using language '%1'").arg(matched));
    return matched;
}

void I18n::restartApplication()
{
    const QString appPath = QCoreApplication::applicationFilePath();
    const QStringList args = QCoreApplication::arguments().mid(1);
    LOG_INFO(QString("I18n: restarting %1 to apply language change").arg(appPath));
    if (QProcess::startDetached(appPath, args))
    {
        // Give the new instance a moment to start before we tear down.
        QTimer::singleShot(200, qApp, &QCoreApplication::quit);
    }
    else
    {
        LOG_ERROR(QString("I18n: failed to restart %1, please start it manually").arg(appPath));
    }
}

