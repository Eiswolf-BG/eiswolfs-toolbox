#include "weather_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "units.h"

namespace WeatherClient {

namespace {
    // Body erst komplett einsammeln statt direkt aus http.getStream() zu
    // parsen (Chunked-Transfer-Encoding - gleiches Muster wie
    // rocket_client.cpp/transit_client.cpp, siehe dortige CLAUDE.md-
    // Begruendung).
    bool httpGet(const String& url, String& outBody) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(Config::HTTP_TIMEOUT_MS);

        HTTPClient http;
        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return false;
        http.addHeader("User-Agent", Config::NOMINATIM_USER_AGENT);

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return false;
        }
        outBody = http.getString();
        http.end();
        return true;
    }

    // Zweiter, eigener HTTPS-Request an eine ANDERE Subdomain
    // (air-quality-api statt api, siehe CLAUDE.md "Wetter-Screen:
    // Luftqualitaet") - laeuft im selben Abruf-Zyklus wie fetch() oben,
    // ist aber technisch zwingend ein separater Request. Ein
    // Fehlschlag hier laesst AirQuality.valid auf false, beeintraechtigt
    // aber NICHT den Rest der Wettervorhersage (Teilausfall-Toleranz,
    // gleiches Prinzip wie bei SpaceWeatherClient::fetchNoaaData zuvor).
    void fetchAirQuality(double lat, double lon, AirQuality& out) {
        char latlon[48];
        snprintf(latlon, sizeof(latlon), "latitude=%.6f&longitude=%.6f", lat, lon);

        String url = "https://" + String(Config::WEATHER_AIR_QUALITY_API_HOST) +
                     String(Config::WEATHER_AIR_QUALITY_PATH) +
                     "?" + String(latlon) +
                     "&current=pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone,"
                     "dust,ammonia,european_aqi,us_aqi,european_aqi_pm2_5,european_aqi_pm10,"
                     "european_aqi_no2,european_aqi_o3,european_aqi_so2";

        String body;
        if (!httpGet(url, body)) return;

        // Feldfilter (live gegen die echte API geprueftes Antwortschema,
        // siehe CLAUDE.md) - alle Felder sind native JSON-Zahlen (ANDERS
        // als bei Jolpica/Formel-1, dort waren vergleichbare Felder
        // JSON-Strings - hier live bestaetigt NICHT der Fall), der
        // ArduinoJson-"|"-Operator mit Zahlen-Default funktioniert daher
        // direkt.
        JsonDocument filter;
        JsonObject c = filter["current"].to<JsonObject>();
        c["pm10"] = true;
        c["pm2_5"] = true;
        c["carbon_monoxide"] = true;
        c["nitrogen_dioxide"] = true;
        c["sulphur_dioxide"] = true;
        c["ozone"] = true;
        c["dust"] = true;
        c["ammonia"] = true;
        c["european_aqi"] = true;
        c["us_aqi"] = true;
        c["european_aqi_pm2_5"] = true;
        c["european_aqi_pm10"] = true;
        c["european_aqi_no2"] = true;
        c["european_aqi_o3"] = true;
        c["european_aqi_so2"] = true;

        JsonDocument doc;
        if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

        JsonObject current = doc["current"];
        if (current.isNull()) return;

        out.pm10 = current["pm10"] | 0.0f;
        out.pm25 = current["pm2_5"] | 0.0f;
        out.carbonMonoxide = current["carbon_monoxide"] | 0.0f;
        out.nitrogenDioxide = current["nitrogen_dioxide"] | 0.0f;
        out.sulphurDioxide = current["sulphur_dioxide"] | 0.0f;
        out.ozone = current["ozone"] | 0.0f;
        out.dust = current["dust"] | 0.0f;
        out.ammonia = current["ammonia"] | 0.0f;
        // -1 = nicht verfuegbar (API liefert dafuer null, je nach Messnetz-
        // Abdeckung moeglich, siehe weather_client.h) - "| -1" greift nur,
        // wenn das Feld WIRKLICH fehlt/null ist, nicht bei 0.
        out.europeanAqi = (int16_t)(int)(current["european_aqi"] | -1);
        out.usAqi = (int16_t)(int)(current["us_aqi"] | -1);
        out.aqiPm25 = (int16_t)(int)(current["european_aqi_pm2_5"] | -1);
        out.aqiPm10 = (int16_t)(int)(current["european_aqi_pm10"] | -1);
        out.aqiNo2 = (int16_t)(int)(current["european_aqi_no2"] | -1);
        out.aqiO3 = (int16_t)(int)(current["european_aqi_o3"] | -1);
        out.aqiSo2 = (int16_t)(int)(current["european_aqi_so2"] | -1);

        out.valid = out.europeanAqi >= 0;
    }
}

FetchResult fetch(double lat, double lon, Forecast& out) {
    char latlon[48];
    snprintf(latlon, sizeof(latlon), "latitude=%.6f&longitude=%.6f", lat, lon);

    // "timezone=auto" liefert alle Zeit-Strings bereits als lokale
    // Wanduhrzeit (kein "Z"-Suffix, DST automatisch beruecksichtigt) - live
    // bestaetigt, siehe CLAUDE.md "Wetter-Screen". forecast_hours begrenzt
    // NUR das stuendliche Array, forecast_days NUR das taegliche - beide
    // unabhaengig voneinander (live bestaetigt).
    // temperature_unit/wind_speed_unit/precipitation_unit (Alex' Auftrag,
    // 07.10., siehe CLAUDE.md "Globale Einheiten-Einstellung") - Open-
    // Meteo rechnet serverseitig um (live per curl verifiziert), der
    // Client rechnet selbst NICHTS um.
    String url = "https://" + String(Config::WEATHER_API_HOST) + String(Config::WEATHER_FORECAST_PATH) +
                 "?" + String(latlon) +
                 "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,"
                 "wind_speed_10m,wind_direction_10m,precipitation,is_day"
                 "&hourly=temperature_2m,precipitation_probability,weather_code"
                 "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,"
                 "precipitation_probability_max,wind_speed_10m_max,uv_index_max,sunrise,sunset"
                 "&timezone=auto"
                 "&temperature_unit=" + String(Units::temperatureUnitParam()) +
                 "&wind_speed_unit=" + String(Units::windSpeedUnitParam()) +
                 "&precipitation_unit=" + String(Units::precipitationUnitParam()) +
                 "&forecast_days=" + String(Config::WEATHER_FORECAST_DAYS) +
                 "&forecast_hours=" + String(Config::WEATHER_FORECAST_HOURS);

    String body;
    if (!httpGet(url, body)) return FetchResult::NetworkError;

    // Feldfilter (live gegen die echte API geprueftes Antwortschema, siehe
    // CLAUDE.md) - "current"/"hourly"/"daily" sind einfache Objekte bzw.
    // Objekte aus parallelen Arrays (kein Array-von-Objekten wie bei
    // Transitous), deshalb reicht ein direkter Feldname ohne "[0]".
    JsonDocument filter;
    filter["current"]["time"] = true;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["apparent_temperature"] = true;
    filter["current"]["relative_humidity_2m"] = true;
    filter["current"]["weather_code"] = true;
    filter["current"]["wind_speed_10m"] = true;
    filter["current"]["wind_direction_10m"] = true;
    filter["current"]["precipitation"] = true;
    filter["current"]["is_day"] = true;
    filter["hourly"]["time"] = true;
    filter["hourly"]["temperature_2m"] = true;
    filter["hourly"]["precipitation_probability"] = true;
    filter["hourly"]["weather_code"] = true;
    filter["daily"]["time"] = true;
    filter["daily"]["weather_code"] = true;
    filter["daily"]["temperature_2m_max"] = true;
    filter["daily"]["temperature_2m_min"] = true;
    filter["daily"]["precipitation_sum"] = true;
    filter["daily"]["precipitation_probability_max"] = true;
    filter["daily"]["wind_speed_10m_max"] = true;
    filter["daily"]["uv_index_max"] = true;
    filter["daily"]["sunrise"] = true;
    filter["daily"]["sunset"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) return FetchResult::ParseError;

    JsonObject current = doc["current"];
    if (current.isNull()) return FetchResult::ParseError;
    out.currentTimeLocal = String((const char*)(current["time"] | ""));
    out.temperature = current["temperature_2m"] | 0.0f;
    out.apparentTemperature = current["apparent_temperature"] | 0.0f;
    out.humidityPercent = current["relative_humidity_2m"] | 0;
    out.weatherCode = current["weather_code"] | 0;
    out.windSpeedKmh = current["wind_speed_10m"] | 0.0f;
    out.windDirectionDeg = current["wind_direction_10m"] | 0;
    out.precipitationMm = current["precipitation"] | 0.0f;
    out.isDay = (int)(current["is_day"] | 1) != 0;

    JsonObject hourly = doc["hourly"];
    out.hourlyCount = 0;
    if (!hourly.isNull()) {
        JsonArray hTime = hourly["time"].as<JsonArray>();
        JsonArray hTemp = hourly["temperature_2m"].as<JsonArray>();
        JsonArray hPrecip = hourly["precipitation_probability"].as<JsonArray>();
        JsonArray hCode = hourly["weather_code"].as<JsonArray>();
        if (!hTime.isNull()) {
            for (size_t i = 0; i < hTime.size() && out.hourlyCount < Config::WEATHER_MAX_HOURLY; i++) {
                HourlyPoint& h = out.hourly[out.hourlyCount];
                h.timeLocal = String((const char*)(hTime[i] | ""));
                h.temperature = hTemp[i] | 0.0f;
                h.precipProbability = hPrecip[i] | 0;
                h.weatherCode = hCode[i] | 0;
                out.hourlyCount++;
            }
        }
    }

    JsonObject daily = doc["daily"];
    out.dailyCount = 0;
    if (!daily.isNull()) {
        JsonArray dTime = daily["time"].as<JsonArray>();
        JsonArray dCode = daily["weather_code"].as<JsonArray>();
        JsonArray dMax = daily["temperature_2m_max"].as<JsonArray>();
        JsonArray dMin = daily["temperature_2m_min"].as<JsonArray>();
        JsonArray dPrecipSum = daily["precipitation_sum"].as<JsonArray>();
        JsonArray dPrecipProb = daily["precipitation_probability_max"].as<JsonArray>();
        JsonArray dWind = daily["wind_speed_10m_max"].as<JsonArray>();
        JsonArray dUv = daily["uv_index_max"].as<JsonArray>();
        JsonArray dSunrise = daily["sunrise"].as<JsonArray>();
        JsonArray dSunset = daily["sunset"].as<JsonArray>();
        if (!dTime.isNull()) {
            for (size_t i = 0; i < dTime.size() && out.dailyCount < Config::WEATHER_MAX_DAILY; i++) {
                DailyPoint& d = out.daily[out.dailyCount];
                d.dateLocal = String((const char*)(dTime[i] | ""));
                d.weatherCode = dCode[i] | 0;
                d.tempMax = dMax[i] | 0.0f;
                d.tempMin = dMin[i] | 0.0f;
                d.precipSum = dPrecipSum[i] | 0.0f;
                d.precipProbabilityMax = dPrecipProb[i] | 0;
                d.windSpeedMax = dWind[i] | 0.0f;
                d.uvIndexMax = dUv[i] | 0.0f;
                d.sunrise = String((const char*)(dSunrise[i] | ""));
                d.sunset = String((const char*)(dSunset[i] | ""));
                out.dailyCount++;
            }
        }
    }

    if (out.hourlyCount == 0 && out.dailyCount == 0) return FetchResult::ParseError;

    // Luftqualitaet IM SELBEN Abruf-Zyklus (Alex' Auftrag, 06.10.) -
    // zweiter, eigener HTTPS-Request (andere Subdomain, siehe oben). Ein
    // Fehlschlag hier faellt NICHT auf den Rest der Vorhersage zurueck
    // (out.airQuality.valid bleibt einfach false, siehe weather_client.h).
    fetchAirQuality(lat, lon, out.airQuality);

    return FetchResult::Ok;
}

}
