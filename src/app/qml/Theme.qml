pragma Singleton
import QtQuick

// Interface « Atelier sombre » (canevas « Interface Picazoo ») : graphite neutre pour
// laisser les photos ressortir, bleu du logo pour la sélection et les actions,
// ambre du soleil réservé à l'activité en cours.
QtObject {
    // Fonds
    readonly property color bg: "#111317"
    readonly property color panel: "#181B21"
    readonly property color sidebar: "#15181D"
    readonly property color surface: "#1F232A"
    readonly property color field: "#0E1013"
    readonly property color line: "#2A2F38"
    readonly property color lineStrong: "#343A45"
    readonly property color hover: "#232833"

    // Texte (contrastes ≥ 4,5:1 sur les fonds ci-dessus)
    readonly property color text: "#E9ECF1"
    readonly property color textSoft: "#C4CAD3"
    readonly property color text2: "#A9B1BD"
    readonly property color text3: "#8E97A4"

    // Accent (bleu du logo, éclairci pour le fond sombre)
    readonly property color accent: "#3D8BD9"
    readonly property color accentButton: "#2F7BC8"
    readonly property color accentBg: "#1C2C40"
    readonly property color accentText: "#9CC6EE"

    // Activité (ambre du soleil)
    readonly property color amber: "#F2A93B"
    readonly property color amberText: "#F4C77A"
    readonly property color amberBg: "#241F14"
    readonly property color amberLine: "#4A3A1A"
    readonly property color amberTrack: "#3A3020"

    // États
    readonly property color okBg: "#16302A"
    readonly property color okText: "#8FD6B8"
    readonly property color errBg: "#3A1C1A"
    readonly property color errText: "#F1A9A0"

    // Visionneuse
    readonly property color viewerBg: "#0B0C0F"
    readonly property color viewerPanel: "#121418"
    readonly property color viewerLine: "#23272F"

    // Typographie : Segoe UI (Windows) ; titres en Segoe UI Variable Display s'il existe.
    readonly property string font: "Segoe UI"
    readonly property string displayFont: Qt.fontFamilies().indexOf("Segoe UI Variable Display") >= 0
                                          ? "Segoe UI Variable Display" : "Segoe UI"
    readonly property int radius: 10

    // Icônes : police d'icônes de Windows (Fluent sous Windows 11, MDL2 sous Windows 10 ;
    // mêmes points de code).
    readonly property string iconFont: Qt.fontFamilies().indexOf("Segoe Fluent Icons") >= 0
                                       ? "Segoe Fluent Icons" : "Segoe MDL2 Assets"
    readonly property string iconFolder: ""
    readonly property string iconFullScreen: ""
    readonly property string iconExitFullScreen: "" // BackToWindow (007)
    readonly property string iconInfo:""
    readonly property string iconBack: ""
    readonly property string iconPrev: ""
    readonly property string iconNext: ""
    readonly property string iconPause: ""
    readonly property string iconPlay: ""
    readonly property string iconGrid: ""
    readonly property string iconAdd: ""

    // Images liées (003)
    readonly property string iconRelated: "\uE71B" // Link

    // Doublons (002)
    readonly property string iconDuplicates: ""
    readonly property string iconStar: ""
    readonly property string iconDelete: ""
    readonly property string iconMore: ""
    readonly property string iconCompare: ""
    readonly property string iconUndo: ""
}
