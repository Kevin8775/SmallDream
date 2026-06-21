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

        case TextId::CoinDialogueTitle: return es ? "Las Monedas del Sue\u00f1o" : "The Dream Coins";
        case TextId::CoinDialoguePage1: return es ? "Encuentras 3 monedas oscuras esparcidas por la habitaci\u00f3n.\nParecen haber perdido su brillo..." : "You find 3 dark coins scattered around the room.\nThey seem to have lost their shine...";
        case TextId::CoinDialoguePage2: return es ? "Para aclararlas, debes superar un desaf\u00edo junto a cada una.\nCada moneda tiene un reto diferente." : "To lighten them, you must overcome a challenge near each one.\nEach coin has a different trial.";
        case TextId::CoinDialoguePage3: return es ? "Una vez que las tengas todas, podr\u00e1s salir de la habitaci\u00f3n.\nBusca las monedas y presiona E para interactuar." : "Once you have them all, you can leave the room.\nFind the coins and press E to interact.";
        case TextId::CoinCounter: return es ? "Monedas: " : "Coins: ";
        case TextId::CoinApproachMsg: return es ? "Presiona E para intentar aclarar la moneda" : "Press E to try to lighten the coin";
        case TextId::MinigameWin: return es ? "\u00a1Moneda aclarada!" : "Coin lightened!";
        case TextId::MinigameLose: return es ? "Fallaste... intenta de nuevo" : "Failed... try again";
        case TextId::CoinAllCollected: return es ? "\u00a1Todas las monedas obtenidas! La puerta est\u00e1 desbloqueada" : "All coins collected! The door is unlocked";
        case TextId::BedroomExitMsg: return es ? "Presiona E para salir del cuarto" : "Press E to leave the room";
        case TextId::BedroomExitLocked: return es ? "Necesitas las 3 monedas para salir" : "You need 3 coins to leave";
        case TextId::MinigameTimingTitle: return es ? "Reflejos" : "Reflexes";
        case TextId::MinigameTimingInstruction: return es ? "Presiona SPACE cuando la barra est\u00e9 en la zona verde" : "Press SPACE when the bar is in the green zone";
        case TextId::MinigameAttempts: return es ? "Intentos: " : "Attempts: ";
        case TextId::MinigameMemoryTitle: return es ? "Memoria" : "Memory";
        case TextId::MinigameMemoryPairs: return es ? "Pares: " : "Pairs: ";
        case TextId::MinigamePatternTitle: return es ? "Memoriza el Patr\u00f3n" : "Memorize the Pattern";
        case TextId::MinigamePatternObserve: return es ? "Observa la secuencia..." : "Watch the sequence...";
        case TextId::MinigamePatternRepeat: return es ? "Repite la secuencia" : "Repeat the sequence";
        case TextId::MinigamePatternProgress: return es ? "/6" : "/6";
        case TextId::MinigameMatchFound: return es ? "\u00a1Par encontrado!" : "Pair found!";
        case TextId::MinigameSpeedLabel: return es ? "Velocidad: " : "Speed: ";
        case TextId::MinigameSequenceShow: return es ? "Observa la secuencia..." : "Watch the sequence...";
        case TextId::MinigameSequenceRepeat: return es ? "Repite la secuencia" : "Repeat the sequence";
        case TextId::CoinDialogueHint: return es ? "Click para continuar" : "Click to continue";
        case TextId::CoinDialogueStart: return es ? "Click para empezar" : "Click to start";
        case TextId::MinigamePerfect: return es ? "\u00a1Perfecto!" : "Perfect!";
        case TextId::MinigameCombo: return es ? "Combo x" : "Combo x";
        case TextId::MinigameLevel: return es ? "Nivel " : "Level ";
        case TextId::MinigamePreview: return es ? "Memoriza las cartas..." : "Memorize the cards...";
        case TextId::MinigameGameOver: return es ? "Juego terminado" : "Game Over";
        case TextId::MinigameRetry: return es ? "Presiona SPACE para reintentar" : "Press SPACE to retry";
        case TextId::MinigameQuickTapTitle: return es ? "Reacci\u00f3n R\u00e1pida" : "Quick Reaction";
        case TextId::MinigameQuickTapInstruction: return es ? "Haz clic en los objetivos antes de que desaparezcan" : "Click targets before they disappear";
        case TextId::MinigameQuickTapHits: return es ? "Aciertos: " : "Hits: ";
        case TextId::MinigameColorMatchTitle: return es ? "Coincidencia de Colores" : "Color Match";
        case TextId::MinigameColorMatchInstruction: return es ? "Haz clic en el color que coincida con el objetivo" : "Click the color that matches the target";
        case TextId::MinigameColorMatchRound: return es ? "Ronda " : "Round ";
        case TextId::MinigameColorMatchScore: return es ? "Puntos: " : "Score: ";
        case TextId::MinigameSequenceTitle: return es ? "Memoriza la Secuencia" : "Memorize the Sequence";
        case TextId::MinigameSequenceInstruction: return es ? "Observa los n\u00fameros y repite el orden" : "Watch the numbers and repeat the order";
        case TextId::MinigameSequenceObserve: return es ? "Observa..." : "Watch...";
        case TextId::MinigameTutorialStart: return es ? "Presiona CLICK para comenzar" : "Press CLICK to start";
        case TextId::MinigameTutorialQuickTap: return es ? "Haz click en las formas que aparezcan.\nTienes que acertar 8 de 12 antes de que desaparezcan." : "Click on the shapes that appear.\nYou need to hit 8 out of 12 before they disappear.";
        case TextId::MinigameTutorialColorMatch: return es ? "Encuentra el color que coincide con el objetivo.\nNecesitas 4 rondas correctas de 6." : "Find the color that matches the target.\nYou need 4 correct rounds out of 6.";
        case TextId::MinigameTutorialSequence: return es ? "Observa las formas que se iluminan.\nRepite el orden haciendo click. Completa 3 niveles." : "Watch the shapes that light up.\nRepeat the order by clicking. Complete 3 levels.";
        case TextId::MinigameExitHint: return es ? "ESC = Salir" : "ESC = Exit";
        case TextId::MinigameFindColor: return es ? "!ENCUENTRA ESTE COLOR!" : "FIND THIS COLOR!";
        case TextId::MinigameYourTurn: return es ? "!TU TURNO!" : "YOUR TURN!";
        case TextId::MinigameTargets: return es ? "Objetivos: " : "Targets: ";
        case TextId::MinigameTapToStart: return es ? "CLICK para empezar" : "CLICK to start";
        case TextId::MinigameCountdownGo: return es ? "!YA!" : "GO!";
    }
    return "";
}

}
