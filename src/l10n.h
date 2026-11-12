#pragma once

#include <QString>

namespace l10n {

// Aktiviert die Oberflaechensprache ("de" oder "en").
void setLanguage(const QString& code);

// Aktuelle Sprache ("de" oder "en").
QString language();

// Uebersetzung fuer einen Schluessel; faellt auf den Schluessel zurueck.
QString L(const QString& key);

} // namespace l10n
