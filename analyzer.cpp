#include "analyzer.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <cctype>



static std::unordered_map<std::string, long long> zoneCounts;
static std::unordered_map<std::string, std::array<long long, 24>> slotCountsByZone;



static bool parseHourFast(const std::string& datetime, int& hour) {
    // Expected format: YYYY-MM-DD HH:MM
    if (datetime.size() < 13) return false;

    unsigned char c1 = static_cast<unsigned char>(datetime[11]);
    unsigned char c2 = static_cast<unsigned char>(datetime[12]);

    if (!std::isdigit(c1) || !std::isdigit(c2)) return false;

    hour = (datetime[11] - '0') * 10 + (datetime[12] - '0');
    return (0 <= hour && hour <= 23);
}

static bool parseLine(const std::string& line, std::string& zone, int& hour) {
    // CSV schema:
    // TripID,PickupZoneID,DropoffZoneID,PickupDateTime,DistanceKm,FareAmount

    std::stringstream ss(line);
    std::string token;
    std::string datetime;

    // TripID
    if (!std::getline(ss, token, ',')) return false;

    // PickupZoneID
    if (!std::getline(ss, zone, ',')) return false;
    if (zone.empty()) return false;

    // DropoffZoneID (ignore)
    if (!std::getline(ss, token, ',')) return false;

    // PickupDateTime
    if (!std::getline(ss, datetime, ',')) return false;

    // DistanceKm / FareAmount önemli değil → eksik olabilir

    if (!parseHourFast(datetime, hour)) return false;

    return true;
}


void TripAnalyzer::ingestFile(const std::string& csvPath) {
    // Reset state 
    zoneCounts.clear();
    slotCountsByZone.clear();

    std::ifstream fin(csvPath);
    if (!fin.is_open()) {
        // Must never crash
        return;
    }

    std::string line;

    // Skip header row
    if (!std::getline(fin, line)) return;

    while (std::getline(fin, line)) {
        if (line.empty()) continue;

        std::string zone;
        int hour = -1;

        if (!parseLine(line, zone, hour)) {
            // Dirty row → skip
            continue;
        }

        zoneCounts[zone]++;

        auto& arr = slotCountsByZone[zone]; // auto zero-init
        arr[hour]++;
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    std::vector<ZoneCount> result;
    result.reserve(zoneCounts.size());

    for (const auto& p : zoneCounts) {
        result.push_back({p.first, p.second});
    }

    auto cmp = [](const ZoneCount& a, const ZoneCount& b) {
        if (a.count != b.count) return a.count > b.count; // desc
        return a.zone < b.zone;                           // asc
    };

    if (k < 0) k = 0;

    if ((size_t)k < result.size()) {
        std::partial_sort(result.begin(), result.begin() + k, result.end(), cmp);
        result.resize(k);
    } else {
        std::sort(result.begin(), result.end(), cmp);
    }

    return result;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    std::vector<SlotCount> result;

    for (const auto& p : slotCountsByZone) {
        const std::string& zone = p.first;
        const auto& arr = p.second;

        for (int hour = 0; hour < 24; ++hour) {
            if (arr[hour] > 0) {
                result.push_back({zone, hour, arr[hour]});
            }
        }
    }

    auto cmp = [](const SlotCount& a, const SlotCount& b) {
        if (a.count != b.count) return a.count > b.count;
        if (a.zone != b.zone) return a.zone < b.zone;
        return a.hour < b.hour;
    };

    if (k < 0) k = 0;

    if ((size_t)k < result.size()) {
        std::partial_sort(result.begin(), result.begin() + k, result.end(), cmp);
        result.resize(k);
    } else {
        std::sort(result.begin(), result.end(), cmp);
    }

    return result;
}

