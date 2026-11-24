// SPDX-License-Identifier: MIT
#pragma once
namespace ss { namespace geo {
double distance_m(double lat1, double lon1, double lat2, double lon2);   // haversine, WGS-84 mean radius
double bearing_deg(double lat1, double lon1, double lat2, double lon2);  // initial bearing, 0..360, 0 = north
const char* cardinal(double bearing);                                    // "N","NE",...
double relative_bearing(double bearing, double heading);                 // -180..180, + = turn right
}} 
