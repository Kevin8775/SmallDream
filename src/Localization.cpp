#include "Localization.h"

#include <fstream>

namespace {
Language gLanguage = Language::Spanish;

const char* toCode(Language lang) {
    return lang == Language::Spanish ? "es" : "en";
}

std::string fromCode(const std::string& code) {
    if (code == "en" || code == "EN") return "en";
    if (code == "es" || code == "ES") return "es";
    return "es";
}
}

namespace Localization {

Language current() {
    return gLanguage;
}

void set(Language lang) {
    gLanguage = lang;
}

void toggle() {
    gLanguage = (gLanguage == Language::Spanish) ? Language::English : Language::Spanish;
}

Language load(const std::string& filePath) {
    std::ifstream file(filePath);
    std::string code;
    if (file >> code) {
        code = fromCode(code);
        gLanguage = (code == "en") ? Language::English : Language::Spanish;
    } else {
        gLanguage = Language::Spanish;
    }
    return gLanguage;
}

void save(const std::string& filePath) {
    std::ofstream file(filePath, std::ios::trunc);
    if (!file.is_open()) return;
    file << toCode(gLanguage) << '\n';
}

std::string languageLabel(Language lang) {
    return lang == Language::Spanish ? "Español" : "English";
}

std::string t(TextId id) {
    const bool es = (gLanguage == Language::Spanish);
    switch (id) {
        case TextId::MenuNewDream: return es ? "Nuevo sue\u00f1o" : "New Dream";
        case TextId::MenuContinueExploring: return es ? "Continuar explorando" : "Continue Exploring";
        case TextId::MenuControls: return es ? "Controles" : "Controls";
        case TextId::MenuCredits: return es ? "Cr\u00e9ditos" : "Credits";
        case TextId::MenuLanguage: return (es ? std::string("Idioma: ") : std::string("Language: ")) + languageLabel(gLanguage);
        case TextId::MenuExit: return es ? "Salir" : "Exit";
        case TextId::LoadingResources: return es ? "CARGANDO RECURSOS..." : "LOADING RESOURCES...";
        case TextId::ChooseHowToStart: return es ? "Elige c\u00f3mo empezar" : "Choose how to start";
        case TextId::SkipStory: return es ? "Saltar historia" : "Skip Story";
        case TextId::ViewStory: return es ? "Ver historia" : "View Story";
        case TextId::ControlsTitle: return es ? "CONTROLES" : "CONTROLS";
        case TextId::ControlsWalkStrafe: return es ? "Moverse / Desplazarse" : "Walk / Strafe";
        case TextId::ControlsLookAround: return es ? "Mirar alrededor" : "Look around";
        case TextId::ControlsJump: return es ? "Saltar" : "Jump";
        case TextId::CreditsBackHint: return es ? "Haz clic o presiona ESC para volver" : "Click or press ESC to go back";
        case TextId::ControlsBackHint: return es ? "Haz clic en cualquier parte para volver" : "Click anywhere to go back";
        case TextId::VersionAlpha: return es ? "VERSI\u00d3N 0.9a (BUILD ALPHA)" : "VERSION 0.9a (ALPHA BUILD)";
        case TextId::CreditsMadeBy: return es ? "Hecho por Torres-Chavez-Torrez" : "Made by Torres-Chavez-Torrez";
        case TextId::CreditsTitle: return es ? "CR\u00c9DITOS" : "CREDITS";
        case TextId::CreditsTeam: return es ? "Equipo" : "Team";
        case TextId::CreditsProgram: return es ? "Programa" : "Program";
        case TextId::CreditsUniversity: return es ? "Universidad Nacional de Ingenier\u00eda (UNI)" : "National University of Engineering (UNI)";
        case TextId::CreditsDegree: return es ? "Ingenier\u00eda en Computaci\u00f3n" : "Computer Engineering";
        case TextId::CreditsCourseProject: return es ? "Proyecto del curso de Programaci\u00f3n Gr\u00e1fica" : "Graphic Programming course project";
        case TextId::CreditsThanks: return es ? "Gracias por jugar" : "Thank you for playing";
        case TextId::HousePreparingScene: return es ? "Preparando escena..." : "Preparing scene...";
        case TextId::HouseFailedLoad: return es ? "No se pudo cargar el modelo de la casa" : "Failed to load house model";
        case TextId::HouseFailedError: return es ? "Error al cargar el modelo" : "Model load error";
        case TextId::PauseTitle: return es ? "PAUSA" : "PAUSED";
        case TextId::PauseResume: return es ? "Reanudar" : "Resume";
        case TextId::PauseReturnMenu: return es ? "Volver al inicio" : "Return to menu";

        case TextId::VN_S0_0: return es ? "Otro d\u00eda frente a la misma pantalla." : "Another day in front of the same screen.";
        case TextId::VN_S0_1: return es ? "Las mismas teclas." : "The same keys.";
        case TextId::VN_S0_2: return es ? "Los mismos pasillos." : "The same hallways.";
        case TextId::VN_S0_3: return es ? "Las mismas luces que nunca parec\u00edan apagarse." : "The same lights that never seemed to go out.";
        case TextId::VN_S0_4: return es ? "Mir\u00e9 el reloj." : "I looked at the clock.";
        case TextId::VN_S0_5: return es ? "22:47." : "22:47.";
        case TextId::VN_S0_6: return es ? "Ya era tarde." : "It was already late.";
        case TextId::VN_S0_7: return es ? "Guard\u00e9 los \u00faltimos archivos." : "I saved the last files.";
        case TextId::VN_S0_8: return es ? "Apagu\u00e9 la pantalla." : "I turned off the screen.";
        case TextId::VN_S0_9: return es ? "Y por fin me levant\u00e9." : "And I finally got up.";
        case TextId::VN_S0_10: return es ? "*Empujo la silla hacia atr\u00e1s.*" : "*I push the chair back.*";
        case TextId::VN_S0_11: return es ? "*Agarro mi mochila.*" : "*I grab my backpack.*";
        case TextId::VN_S0_12: return es ? "...y me voy a casa." : "...and I head home.";

        case TextId::VN_S1_0: return es ? "*Las puertas de vidrio se abren lentamente.*" : "*The glass doors slide open slowly.*";
        case TextId::VN_S1_1: return es ? "*Una corriente de aire fresco me golpea la cara.*" : "*A stream of fresh air hits my face.*";
        case TextId::VN_S1_2: return es ? "Mucho mejor." : "Much better.";
        case TextId::VN_S1_3: return es ? "*Empiezo a caminar por la calle.*" : "*I start walking down the street.*";
        case TextId::VN_S1_4: return es ? "Solo quer\u00eda llegar a casa." : "I just wanted to get home.";
        case TextId::VN_S1_5: return es ? "Nada m\u00e1s." : "Nothing else.";

        case TextId::VN_S2_0: return es ? "*Las farolas calientan la acera con un brillo suave.*" : "*The streetlights warm the sidewalk with a soft glow.*";
        case TextId::VN_S2_1: return es ? "*La ciudad parece m\u00e1s silenciosa de lo normal.*" : "*The city seems quieter than usual.*";
        case TextId::VN_S2_2: return es ? "Tan silenciosa..." : "So quiet...";
        case TextId::VN_S2_3: return es ? "*Sigo caminando.*" : "*I keep walking.*";
        case TextId::VN_S2_4: return es ? "*El sonido de mis pasos resuena en la calle vac\u00eda.*" : "*The sound of my footsteps echoes in the empty street.*";
        case TextId::VN_S2_5: return es ? "Supongo que ya es bastante tarde." : "I guess it's already pretty late.";
        case TextId::VN_S2_6: return es ? "*Miro hacia el cielo nocturno.*" : "*I look up at the night sky.*";
        case TextId::VN_S2_7: return es ? "No recuerdo la \u00faltima vez que estuve fuera tan tarde." : "I don't remember the last time I was out this late.";
        case TextId::VN_S2_8: return es ? "*Una brisa fr\u00eda recorre la calle.*" : "*A cold breeze sweeps through the street.*";
        case TextId::VN_S2_9: return es ? "Solo quiero llegar a casa." : "I just want to get home.";
        case TextId::VN_S2_10: return es ? "Descansar un poco." : "Get some rest.";
        case TextId::VN_S2_11: return es ? "*La silueta de mi casa aparece al final de la calle.*" : "*The silhouette of my house appears at the end of the street.*";

        case TextId::VN_S3_0: return es ? "*Abro la puerta y entro.*" : "*I open the door and step inside.*";
        case TextId::VN_S3_1: return es ? "*El silencio me recibe al instante.*" : "*Silence greets me at once.*";
        case TextId::VN_S3_2: return es ? "*Dejo las llaves sobre la mesa.*" : "*I leave the keys on the table.*";
        case TextId::VN_S3_3: return es ? "*La l\u00e1mpara de la sala proyecta una luz suave sobre la habitaci\u00f3n.*" : "*The living room lamp casts a gentle light across the room.*";
        case TextId::VN_S3_4: return es ? "Hogar." : "Home.";
        case TextId::VN_S3_5: return es ? "*Dejo caer mi mochila en el suelo.*" : "*I drop my backpack on the floor.*";
        case TextId::VN_S3_6: return es ? "*Mis hombros se sienten m\u00e1s livianos.*" : "*My shoulders feel lighter.*";
        case TextId::VN_S3_7: return es ? "Al fin." : "At last.";
        case TextId::VN_S3_8: return es ? "*Observo la habitaci\u00f3n durante unos segundos.*" : "*I take in the room for a few seconds.*";
        case TextId::VN_S3_9: return es ? "Nunca pens\u00e9 que un lugar tan simple pudiera sentirse tan bien." : "I never thought such a simple place could feel this good.";
        case TextId::VN_S3_10: return es ? "*Camino despacio hacia el sof\u00e1.*" : "*I walk slowly toward the couch.*";
        case TextId::VN_S3_11: return es ? "Solo necesito descansar un poco." : "I just need to rest a little.";

        case TextId::VN_S4_0: return es ? "*Me dejo caer en el sof\u00e1.*" : "*I let myself fall onto the couch.*";
        case TextId::VN_S4_1: return es ? "*El cansancio del d\u00eda me alcanza de golpe.*" : "*The exhaustion of the day hits me all at once.*";
        case TextId::VN_S4_2: return es ? "Ah..." : "Ah...";
        case TextId::VN_S4_3: return es ? "*Apoyo la cabeza hacia atr\u00e1s.*" : "*I rest my head back.*";
        case TextId::VN_S4_4: return es ? "*Cierro los ojos por un momento.*" : "*I close my eyes for a moment.*";
        case TextId::VN_S4_5: return es ? "Solo unos minutos." : "Just a few minutes.";
        case TextId::VN_S4_6: return es ? "Luego me levantar\u00e9." : "Then I'll get up.";
        case TextId::VN_S4_7: return es ? "Eso era lo que siempre me dec\u00eda." : "That's what I always told myself.";
        case TextId::VN_S4_8: return es ? "*El silencio llena la habitaci\u00f3n.*" : "*Silence fills the room.*";
        case TextId::VN_S4_9: return es ? "*Poco a poco, todo empieza a desvanecerse.*" : "*Little by little, everything begins to fade away.*";
        case TextId::VN_S4_10: return es ? "" : "";

        case TextId::VN_S5_0: return es ? "..." : "...";
        case TextId::VN_S5_1: return es ? "..." : "...";
        case TextId::VN_S5_2: return es ? "*La habitaci\u00f3n se siente extra\u00f1amente silenciosa.*" : "*The room feels strangely silent.*";
        case TextId::VN_S5_3: return es ? "..." : "...";
        case TextId::VN_S5_4: return es ? "*Por un momento, todo se siente distante.*" : "*For a moment, everything feels distant.*";
        case TextId::VN_S5_5: return es ? "..." : "...";
        case TextId::VN_S5_6: return es ? "..." : "...";

        case TextId::VN_S6_0: return es ? "..." : "...";
        case TextId::VN_S6_1: return es ? "..." : "...";
        case TextId::VN_S6_2: return es ? "...?" : "...?";
        case TextId::HouseIntroDialog: return es
            ? "\u00bfEh...? \u00bfQu\u00e9 pas\u00f3?... Todo est\u00e1 tan oscuro... Y todo es enorme... No, espera... No es que sea enorme... Soy yo... \u00a1Me he encogido! Pero... \u00bfc\u00f3mo? \u00bfQu\u00e9 habr\u00e1 pasado?"
            : "Huh...? What happened?... Everything is so dark... And everything is huge... No, wait... It's not that it's huge... It's me... I've shrunk! But... how? What happened?";
    }
    return "";
}

}
