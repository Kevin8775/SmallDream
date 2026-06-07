#include "TextRenderer.h"
#include "Shader.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>

TextRenderer::TextRenderer(const char* fontPath, unsigned int fontSize, Shader* shader)
    : mShader(shader), mVAO(0), mVBO(0), mProjection(glm::mat4(1.0f)) {

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Failed to init FreeType" << std::endl;
        return;
    }
    FT_Face face;
    if (FT_New_Face(ft, fontPath, 0, &face)) {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
        FT_Done_FreeType(ft);
        return;
    }
    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Obligar a OpenGL a procesar texturas de un solo canal byte por byte
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    FT_UInt glyphIndex;
    FT_ULong charCode = FT_Get_First_Char(face, &glyphIndex);
    while (glyphIndex != 0) {
        if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER)) {
            charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
            continue;
        }
        if (face->glyph->bitmap.width == 0 || face->glyph->bitmap.rows == 0) {
            Character ch = {
                0,
                glm::ivec2(0, 0),
                glm::ivec2(0, 0),
                static_cast<unsigned int>(face->glyph->advance.x)
            };
            mCharacters.insert(std::make_pair((unsigned int)charCode, ch));
            charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
            continue;
        }

        // SOLUCIÓN AL CIZALLADO (TEXTO DOBLADO): Alinear el pitch real de la fila de FreeType
        glPixelStorei(GL_UNPACK_ROW_LENGTH, face->glyph->bitmap.pitch);

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0, GL_RED, GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // Resetear inmediatamente

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character ch = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        mCharacters.insert(std::make_pair((unsigned int)charCode, ch));
        charCode = FT_Get_Next_Char(face, charCode, &glyphIndex);
    }
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Configurar el búfer dinámico de vértices para los quads del texto
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    // Atributo 0: Posición de vértice (X, Y)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // Atributo 1: Coordenadas UV de textura (U, V)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

TextRenderer::~TextRenderer() {
    for (auto& pair : mCharacters) {
        glDeleteTextures(1, &pair.second.textureID);
    }
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
    if (mVBO) glDeleteBuffers(1, &mVBO);
}

void TextRenderer::renderText(const std::string& text, float x, float y, float scale, const glm::vec3& color, float alpha) {
    mShader->use();
    mShader->setMat4("uProjection", &mProjection[0][0]);
    mShader->setVec4("uTextColor", color.r, color.g, color.b, alpha);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(mVAO);

    const char* ptr = text.data();
    const char* end = ptr + text.size();
    while (ptr < end) {
        int cp = decodeUTF8(ptr, end);
        if (cp < 0) continue;
        auto it = mCharacters.find(cp);
        if (it == mCharacters.end()) continue;
        Character ch = it->second;

        if (ch.textureID == 0) {
            x += (ch.advance >> 6) * scale;
            continue;
        }

        float xpos = x + ch.bearing.x * scale;

        // SOLUCIÓN AL POSICIONAMIENTO EN PERSPECTIVA / ALINEACIÓN HORIZONTAL COHERENTE:
        // Forzar una línea base sólida compatible con tu matriz glm::ortho Y-Down.
        float baseline = y + (48.0f * scale);
        float ypos = baseline - ch.bearing.y * scale;

        float w = ch.size.x * scale;
        float h = ch.size.y * scale;

        // SOLUCIÓN AL ESPEJADO: Mapeo nativo de UVs de izquierda a derecha (0.0f a 1.0f)
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 1.0f },
            { xpos,     ypos,       0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 0.0f },
            { xpos,     ypos + h,   0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 0.0f },
            { xpos + w, ypos + h,   1.0f, 1.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.advance >> 6) * scale;
    }
    glBindVertexArray(0);
}

glm::vec2 TextRenderer::getTextSize(const std::string& text, float scale) {
    float width = 0, maxHeight = 0;
    const char* ptr = text.data();
    const char* end = ptr + text.size();
    while (ptr < end) {
        int cp = decodeUTF8(ptr, end);
        if (cp < 0) continue;
        auto it = mCharacters.find(cp);
        if (it == mCharacters.end()) continue;
        Character ch = it->second;
        width += (ch.advance >> 6) * scale;
        float h = ch.size.y * scale;
        if (h > maxHeight) maxHeight = h;
    }
    return glm::vec2(width, maxHeight);
}

int TextRenderer::decodeUTF8(const char*& ptr, const char* end) {
    if (ptr >= end) return -1;
    unsigned char c = *ptr++;
    if (c < 0x80) return c;
    int remaining = 0, value = 0;
    if ((c & 0xE0) == 0xC0) { value = c & 0x1F; remaining = 1; }
    else if ((c & 0xF0) == 0xE0) { value = c & 0x0F; remaining = 2; }
    else if ((c & 0xF8) == 0xF0) { value = c & 0x07; remaining = 3; }
    else return -1;
    for (int i = 0; i < remaining; i++) {
        if (ptr >= end) return -1;
        unsigned char next = *ptr++;
        if ((next & 0xC0) != 0x80) return -1;
        value = (value << 6) | (next & 0x3F);
    }
    return value;
}
