// SPDX-License-Identifier: MIT
#include "geo.h"
#include <math.h>
namespace ss { namespace geo {
static const double R = 6371008.8, D2R = 0.017453292519943295;
double distance_m(double a1, double o1, double a2, double o2) {
  double p1 = a1 * D2R, p2 = a2 * D2R, dp = (a2 - a1) * D2R, dl = (o2 - o1) * D2R;
  double h = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
  return 2 * R * atan2(sqrt(h), sqrt(1 - h));
}
double bearing_deg(double a1, double o1, double a2, double o2) {
  double p1 = a1 * D2R, p2 = a2 * D2R, dl = (o2 - o1) * D2R;
  double y = sin(dl) * cos(p2), x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
  double b = atan2(y, x) / D2R;
  return b < 0 ? b + 360.0 : b;
}
const char* cardinal(double b) {
  static const char* n[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int i = (int)floor(fmod(b + 22.5 + 360.0, 360.0) / 45.0);
  return n[i & 7];
}
double relative_bearing(double b, double h) {
  double d = fmod(b - h + 540.0, 360.0) - 180.0;
  return d;
}
}}
