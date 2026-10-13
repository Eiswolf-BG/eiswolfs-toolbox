#include "iss_client.h"
#include <math.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace IssClient {

namespace {
    constexpr double DEG2RAD = 0.017453292519943295;
    constexpr double RAD2DEG = 57.29577951308232;
    // Kugelfoermige Naeherung reicht hier locker aus - eine WGS84-
    // Ellipsoid-Korrektur waere auf dem Radar-Display nicht sichtbar.
    constexpr double EARTH_RADIUS_KM = 6378.137;
}

bool fetch(Position& out) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, Config::ISS_API_URL)) {
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        return false;
    }

    out.latitude = doc["latitude"] | 0.0;
    out.longitude = doc["longitude"] | 0.0;
    out.altitudeKm = doc["altitude"] | 0.0;
    out.velocityKmh = doc["velocity"] | 0.0;
    out.visibility = String((const char*)(doc["visibility"] | "unknown"));
    return true;
}

LookAngles computeLookAngles(double observerLatDeg, double observerLonDeg,
                              double satLatDeg, double satLonDeg, double satAltKm) {
    double obsLat = observerLatDeg * DEG2RAD;
    double obsLon = observerLonDeg * DEG2RAD;
    double satLat = satLatDeg * DEG2RAD;
    double satLon = satLonDeg * DEG2RAD;

    double obsR = EARTH_RADIUS_KM; // Beobachter auf Meereshoehe (Hoehe=0)
    double satR = EARTH_RADIUS_KM + satAltKm;

    double obsX = obsR * cos(obsLat) * cos(obsLon);
    double obsY = obsR * cos(obsLat) * sin(obsLon);
    double obsZ = obsR * sin(obsLat);

    double satX = satR * cos(satLat) * cos(satLon);
    double satY = satR * cos(satLat) * sin(satLon);
    double satZ = satR * sin(satLat);

    double dx = satX - obsX;
    double dy = satY - obsY;
    double dz = satZ - obsZ;

    // Rotation ins lokale Ost-Nord-Oben-System (ENU) des Beobachters.
    double east  = -sin(obsLon) * dx + cos(obsLon) * dy;
    double north = -sin(obsLat) * cos(obsLon) * dx - sin(obsLat) * sin(obsLon) * dy + cos(obsLat) * dz;
    double up    =  cos(obsLat) * cos(obsLon) * dx + cos(obsLat) * sin(obsLon) * dy + sin(obsLat) * dz;

    double rangeKm = sqrt(east * east + north * north + up * up);
    double elevationDeg = atan2(up, sqrt(east * east + north * north)) * RAD2DEG;
    double azimuthDeg = atan2(east, north) * RAD2DEG;
    if (azimuthDeg < 0.0) azimuthDeg += 360.0;

    LookAngles result;
    result.azimuthDeg = azimuthDeg;
    result.elevationDeg = elevationDeg;
    result.rangeKm = rangeKm;
    return result;
}

}
