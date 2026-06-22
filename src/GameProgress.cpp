#include "GameProgress.h"

#include <array>
#include <fstream>
#include <string>

namespace {
    std::array<bool, GameProgress::TOTAL_COINS> gCoins = { false, false, false, false };
    std::string gPath = "progress.cfg";

    // Clave usada en el archivo para cada fuente.
    const char* keyFor(CoinSource s) {
        switch (s) {
            case CoinSource::Bano:       return "bano";
            case CoinSource::Bodega:     return "bodega";
            case CoinSource::Dormitorio: return "dormitorio";
            case CoinSource::Garaje:     return "garaje";
        }
        return "";
    }
}

namespace GameProgress {

void load(const std::string& path) {
    gPath = path;
    gCoins.fill(false);

    std::ifstream in(path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        // Formato: clave=0/1
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        bool got = (!val.empty() && val[0] == '1');
        for (int i = 0; i < TOTAL_COINS; ++i) {
            if (key == keyFor(static_cast<CoinSource>(i))) {
                gCoins[i] = got;
                break;
            }
        }
    }
}

void save(const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;
    for (int i = 0; i < TOTAL_COINS; ++i) {
        out << keyFor(static_cast<CoinSource>(i)) << '=' << (gCoins[i] ? 1 : 0) << '\n';
    }
}

void save() {
    save(gPath);
}

void reset() {
    gCoins.fill(false);
    save();
}

bool hasCoin(CoinSource source) {
    return gCoins[static_cast<int>(source)];
}

bool collect(CoinSource source) {
    int idx = static_cast<int>(source);
    if (gCoins[idx]) return false;
    gCoins[idx] = true;
    save();
    return true;
}

int coinCount() {
    int n = 0;
    for (bool c : gCoins) if (c) ++n;
    return n;
}

bool allCollected() {
    return coinCount() >= TOTAL_COINS;
}

const char* sourceName(CoinSource source) {
    switch (source) {
        case CoinSource::Bano:       return "Bathroom";
        case CoinSource::Bodega:     return "Storage";
        case CoinSource::Dormitorio: return "Bedroom";
        case CoinSource::Garaje:     return "Garage";
    }
    return "";
}

} // namespace GameProgress
