#pragma once

#include "library/Types.h"

#include <QStringView>

namespace picazoo {

// Format déduit de l'extension, sans ouvrir le fichier (FR-003, FR-004).
Format formatFromPath(QStringView path);
Kind kindOf(Format);
QString formatName(Format);        // valeur stockée en base : "jpeg", "cr2"…
Format formatFromName(const QString&);
bool isSupported(Format f);

} // namespace picazoo
