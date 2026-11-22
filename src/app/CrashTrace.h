#pragma once

// Développement : avec PICAZOO_CRASH_TRACE=1, un plantage affiche la pile d'appels
// symbolisée sur la sortie d'erreur (dbghelp).

namespace crashtrace {
void install();
}
