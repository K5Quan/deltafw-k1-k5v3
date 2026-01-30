#include "ag_graphics.h"
#include "fonts/NumbersStepanv3.h"
#include "fonts/NumbersStepanv4.h"
#include "fonts/TomThumb.h"
#include "fonts/muHeavy8ptBold.h"
#include "fonts/muMatrix8ptRegular.h"
#include "fonts/symbols.h"
#include "../external/printf/printf.h"
#include <stdlib.h>
#include <string.h>

static Cursor cursor;

static const GFXfont *const fonts[] = {&TomThumb, &MuMatrix8ptRegular,
                                       &muHeavy8ptBold, &dig_11, &dig_14};

#define SWAP(a, b) { int16_t t = a; a = b; b = t; }

void AG_PutPixel(uint8_t x, uint8_t y, uint8_t fill) {
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
    return;
  
  // Mapping for uv-k1:
  // y=0..7 -> gStatusLine
  // y=8..63 -> gFrameBuffer[0..6] (framebuffer lines are 0-indexed corresponding to screen lines 1-7 (8 pixels each))
  
  if (y < 8) {
     uint8_t m = 1 << (y & 7);
     uint8_t *p = &gStatusLine[x];
     *p = fill ? (fill & 2 ? *p ^ m : *p | m) : *p & ~m;
  } else {
     uint8_t fb_y = y - 8;
     uint8_t line = fb_y >> 3; // Divide by 8
     if (line >= FRAME_LINES) return;
     
     uint8_t m = 1 << (fb_y & 7);
     uint8_t *p = &gFrameBuffer[line][x];
     *p = fill ? (fill & 2 ? *p ^ m : *p | m) : *p & ~m;
  }
}

bool AG_GetPixel(uint8_t x, uint8_t y) {
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
    return false;
    
  if (y < 8) {
     return gStatusLine[x] & (1 << (y & 7));
  } else {
     uint8_t fb_y = y - 8;
     uint8_t line = fb_y >> 3;
     if (line >= FRAME_LINES) return false;
     
     return gFrameBuffer[line][x] & (1 << (fb_y & 7));
  }
}

static void AG_DrawALine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t c) {
  int16_t s = abs(y1 - y0) > abs(x1 - x0);
  if (s) {
    SWAP(x0, y0);
    SWAP(x1, y1);
  }
  if (x0 > x1) {
    SWAP(x0, x1);
    SWAP(y0, y1);
  }

  int16_t dx = x1 - x0, dy = abs(y1 - y0), e = dx >> 1, ys = y0 < y1 ? 1 : -1;
  for (; x0 <= x1; x0++, e -= dy) {
    AG_PutPixel(s ? y0 : x0, s ? x0 : y0, c);
    if (e < 0) {
      y0 += ys;
      e += dx;
    }
  }
}

void AG_DrawVLine(int16_t x, int16_t y, int16_t h, Color c) {
  if (h)
    AG_DrawALine(x, y, x, y + h - 1, c);
}

void AG_DrawHLine(int16_t x, int16_t y, int16_t w, Color c) {
  if (w)
    AG_DrawALine(x, y, x + w - 1, y, c);
}

void AG_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color c) {
  if (x0 == x1) {
    if (y0 > y1)
      SWAP(y0, y1);
    AG_DrawVLine(x0, y0, y1 - y0 + 1, c);
  } else if (y0 == y1) {
    if (x0 > x1)
      SWAP(x0, x1);
    AG_DrawHLine(x0, y0, x1 - x0 + 1, c);
  } else
    AG_DrawALine(x0, y0, x1, y1, c);
}

void AG_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, Color c) {
  AG_DrawHLine(x, y, w, c);
  AG_DrawHLine(x, y + h - 1, w, c);
  AG_DrawVLine(x, y, h, c);
  AG_DrawVLine(x + w - 1, y, h, c);
}

void AG_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color c) {
  for (int16_t i = x, e = x + w; i < e; i++)
    AG_DrawVLine(i, y, h, c);
}

static void m_putchar(int16_t x, int16_t y, uint8_t c, Color col, uint8_t sx,
                      uint8_t sy, const GFXfont *f) {
  const GFXglyph *g = &f->glyph[c - f->first];
  const uint8_t *b = f->bitmap + g->bitmapOffset;
  uint8_t w = g->width, h = g->height, bits = 0, bit = 0;
  int8_t xo = g->xOffset, yo = g->yOffset;

  for (uint8_t yy = 0; yy < h; yy++) {
    for (uint8_t xx = 0; xx < w; xx++, bits <<= 1) {
      if (!(bit++ & 7))
        bits = *b++;
      if (bits & 0x80) {
        (sx == 1 && sy == 1)
            ? AG_PutPixel(x + xo + xx, y + yo + yy, col)
            : AG_FillRect(x + (xo + xx) * sx, y + (yo + yy) * sy, sx, sy, col);
      }
    }
  }
}

void charBounds(uint8_t c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny,
                int16_t *maxx, int16_t *maxy, uint8_t tsx, uint8_t tsy,
                bool wrap, const GFXfont *f) {
  if (c == '\n') {
    *x = 0;
    *y += tsy * f->yAdvance;
    return;
  }
  if (c == '\r' || c < f->first || c > f->last)
    return;

  const GFXglyph *g = &f->glyph[c - f->first];
  if (wrap && (*x + ((g->xOffset + g->width) * tsx) > LCD_WIDTH)) {
    *x = 0;
    *y += tsy * f->yAdvance;
  }

  int16_t x1 = *x + g->xOffset * tsx, y1 = *y + g->yOffset * tsy;
  int16_t x2 = x1 + g->width * tsx - 1, y2 = y1 + g->height * tsy - 1;
  if (x1 < *minx)
    *minx = x1;
  if (y1 < *miny)
    *miny = y1;
  if (x2 > *maxx)
    *maxx = x2;
  if (y2 > *maxy)
    *maxy = y2;
  *x += g->xAdvance * tsx;
}

static void getTextBounds(const char *s, int16_t x, int16_t y, int16_t *x1,
                          int16_t *y1, uint16_t *w, uint16_t *h,
                          const GFXfont *f) {
  int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;
  for (; *s; s++)
    charBounds(*s, &x, &y, &minx, &miny, &maxx, &maxy, 1, 1, 0, f);
  *x1 = maxx >= minx ? minx : x;
  *y1 = maxy >= miny ? miny : y;
  *w = maxx >= minx ? maxx - minx + 1 : 0;
  *h = maxy >= miny ? maxy - miny + 1 : 0;
}

void write_char(uint8_t c, uint8_t tsx, uint8_t tsy, bool wrap, Color col,
           const GFXfont *f) {
  if (c == '\n') {
    cursor.x = 0;
    cursor.y += tsy * f->yAdvance;
    return;
  }
  if (c == '\r' || c < f->first || c > f->last)
    return;

  const GFXglyph *g = &f->glyph[c - f->first];
  if (g->width && g->height) {
    if (wrap && (cursor.x + tsx * (g->xOffset + g->width) > LCD_WIDTH)) {
      cursor.x = 0;
      cursor.y += tsy * f->yAdvance;
    }
    m_putchar(cursor.x, cursor.y, c, col, tsx, tsy, f);
  }
  cursor.x += g->xAdvance * tsx;
}

static void printStr(const GFXfont *f, uint8_t x, uint8_t y, Color col,
                     TextPos pos, const char *fmt, va_list args) {
  char buf[64];
  vsnprintf(buf, 64, fmt, args);
  int16_t x1, y1;
  uint16_t w, h;
  getTextBounds(buf, x, y, &x1, &y1, &w, &h, f);
  cursor.x = pos == POS_C ? x - (w >> 1) : pos == POS_R ? x - w : x;
  cursor.y = y;
  for (char *p = buf; *p; p++)
    write_char(*p, 1, 1, 1, col, f);
}

// Macros to generate functions
#define P(n, i)                                                                \
  void AG_Print##n(uint8_t x, uint8_t y, const char *f, ...) {                 \
    va_list a;                                                                 \
    va_start(a, f);                                                            \
    printStr(fonts[i], x, y, C_FILL, POS_L, f, a);                             \
    va_end(a);                                                                 \
  }
#define PX(n, i)                                                               \
  void AG_Print##n##Ex(uint8_t x, uint8_t y, TextPos p, Color c, const char *f,\
                    ...) {                                                     \
    va_list a;                                                                 \
    va_start(a, f);                                                            \
    printStr(fonts[i], x, y, c, p, f, a);                                      \
    va_end(a);                                                                 \
  }

P(Small, 0)
PX(Small, 0) P(Medium, 1) PX(Medium, 1) P(MediumBold, 2) PX(MediumBold, 2)
    P(BigDigits, 3) PX(BigDigits, 3) P(BiggestDigits, 4) PX(BiggestDigits, 4)

void AG_PrintSymbolsEx(uint8_t x, uint8_t y, TextPos p, Color c,
                            const char *f, ...) {
  va_list a;
  va_start(a, f);
  printStr(&Symbols, x, y, c, p, f, a);
  va_end(a);
}
