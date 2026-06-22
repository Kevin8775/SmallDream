#pragma once

#include <string>

// Las cuatro fuentes posibles de moneda. El orden NO debe cambiarse porque se
// usa como indice del arreglo interno y en el archivo de guardado.
enum class CoinSource {
    Bano = 0,        // minijuego del bano
    Bodega = 1,      // minijuego de la bodega
    Dormitorio = 2,  // escape room del dormitorio
    Garaje = 3       // parkour del garaje
};

// Sistema de progreso persistente del jugador. Guarda que monedas se han
// recolectado y de que cuarto provino cada una, de modo que al salir al menu
// (o cerrar el juego) no se pierda la cuenta. Funciona de manera global, igual
// que Localization.
namespace GameProgress {
    constexpr int TOTAL_COINS = 4;

    // Carga el progreso desde el archivo dado y recuerda la ruta para autoguardar.
    void load(const std::string& path);
    // Guarda en la ruta recordada por load().
    void save();
    // Guarda en una ruta especifica.
    void save(const std::string& path);
    // Borra todo el progreso (todas las monedas como no recolectadas) y guarda.
    void reset();

    bool hasCoin(CoinSource source);
    // Marca una moneda como recolectada y autoguarda. Devuelve true si era nueva.
    bool collect(CoinSource source);

    int coinCount();
    bool allCollected();

    // Nombre identificador en ingles (para texto/depuracion).
    const char* sourceName(CoinSource source);
}
