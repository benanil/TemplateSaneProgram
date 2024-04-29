
#pragma once

typedef int FontAtlasHandle;

enum {
    InvalidFontHandle = -1   
};

FontAtlasHandle LoadFontAtlas(const char* file);

void DrawText(const char* text, int txtLen, float xPos, float yPos, float width, float height);

void TextRendererInitialize();

void DestroyTextRenderer();

