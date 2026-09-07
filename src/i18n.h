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

#ifndef I18N_H
#define I18N_H

#include <QString>
#include <QStringList>

//! Application localization helpers (Qt Linguist / QTranslator).
namespace I18n
{
    /// Language codes available in the UI language selector. An empty code
    /// means "follow the system language". Entries are ordered as they
    /// should appear in the menu.
    QStringList availableLanguageCodes();

    /// Language preference currently stored in the configuration
    /// (may be empty, meaning "follow system language").
    QString configuredLanguage();

    /// Install application and Qt translators for the configured language
    /// (or for the system language when the preference is empty).
    /// Must be called after QApplication and CFG->Load() exist.
    /// Returns the language code that was actually applied, e.g. "en" or
    /// "zh_CN" ("" when no translation file was found).
    QString installTranslators();

    /// Restart the running application so a language change fully applies.
    void restartApplication();
}

#endif // I18N_H
