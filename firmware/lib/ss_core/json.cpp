// SPDX-License-Identifier: MIT
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
namespace ss { namespace json {

static const char* ws(const char* p) { while (p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++; return p; }
static const char* skip_str(const char* p) { // p at opening quote
  p++;
  while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
  return *p ? p + 1 : p;
}
const char* skip(const char* p) {
  p = ws(p); if (!p || !*p) return p;
  if (*p == '"') return skip_str(p);
  if (*p == '{' || *p == '[') {
    int depth = 0;
    while (*p) {
      if (*p == '"') { p = skip_str(p); continue; }
      if (*p == '{' || *p == '[') depth++;
      else if (*p == '}' || *p == ']') { if (--depth == 0) return p + 1; }
      p++;
    }
    return p;
  }
  while (*p && *p != ',' && *p != '}' && *p != ']') p++; // number / literal
  return p;
}
const char* find(const char* v, const char* key) {
  v = ws(v); if (!v || *v != '{') return nullptr;
  size_t kl = strlen(key); const char* p = v + 1;
  for (;;) {
    p = ws(p); if (!*p || *p == '}') return nullptr;
    if (*p != '"') return nullptr;
    const char* k = p + 1; const char* e = skip_str(p);
    bool match = (size_t)(e - 1 - k) == kl && !strncmp(k, key, kl);
    p = ws(e); if (*p != ':') return nullptr;
    p = ws(p + 1);
    if (match) return p;
    p = ws(skip(p)); if (*p == ',') p++;
  }
}
const char* path(const char* v, const char* const* keys, int n) {
  for (int i = 0; i < n && v; i++) v = find(v, keys[i]);
  return v;
}
bool num(const char* v, double& out) {
  v = ws(v); if (!v || !*v) return false;
  char* end; double d = strtod(v, &end); if (end == v) return false;
  out = d; return true;
}
bool str(const char* v, char* out, int cap) {
  v = ws(v); if (!v || *v != '"' || cap < 1) return false;
  int o = 0; v++;
  while (*v && *v != '"') {
    char c = *v++;
    if (c == '\\' && *v) { c = *v++; if (c == 'n') c = '\n'; else if (c == 't') c = ' '; }
    if (o < cap - 1) out[o++] = c;
  }
  out[o] = 0; return *v == '"';
}
int nums(const char* v, double* out, int max) {
  v = ws(v); if (!v || *v != '[') return -1;
  int n = 0; const char* p = v + 1;
  for (;;) {
    p = ws(p); if (!*p || *p == ']') return n;
    double d = NAN;
    if (!strncmp(p, "null", 4)) p += 4; else { char* end; d = strtod(p, &end); if (end == p) return n; p = end; }
    if (n < max) out[n] = d;
    n++;
    p = ws(p); if (*p == ',') p++;
  }
}
}}
