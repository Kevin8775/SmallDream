#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct Cuarto {
    std::string rutaModelo;
    glm::vec3 puntoAparicion;
};

struct Puerta {
    glm::vec3 posicion;
    glm::vec3 tamano;
    std::string mensaje;
    Cuarto* destino;
    bool esBodega = false;
};
