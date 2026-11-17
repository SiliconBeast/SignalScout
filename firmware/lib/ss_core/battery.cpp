#include "battery.h"
#include <math.h>
namespace ss { namespace battery {
float vcell_volts(uint16_t r) { return r * 78.125e-6f; }
float soc_percent(uint16_t r) { float s = r / 256.0f; return s > 100 ? 100 : s; }
float crate_pct_per_hr(uint16_t r) { return (int16_t)r * 0.208f; }
float percent_from_voltage(float v) {
  static const float V[] = {3.27f, 3.61f, 3.69f, 3.71f, 3.73f, 3.75f, 3.77f, 3.79f, 3.80f, 3.82f, 3.84f,
                            3.85f, 3.87f, 3.91f, 3.95f, 3.98f, 4.02f, 4.08f, 4.11f, 4.15f, 4.20f};
  const int N = sizeof(V) / sizeof(V[0]);
  if (v <= V[0]) return 0;
  if (v >= V[N - 1]) return 100;
  for (int i = 1; i < N; i++)
    if (v < V[i]) return 5.0f * ((i - 1) + (v - V[i - 1]) / (V[i] - V[i - 1]));
  return 100;
}
int minutes_remaining(float soc, float rate) {
  if (fabsf(rate) < 0.5f) return -1;
  float pct = rate < 0 ? soc : (100.0f - soc);
  return (int)lroundf(pct / fabsf(rate) * 60.0f);
}
}}
