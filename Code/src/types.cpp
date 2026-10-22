#include "types.h"

#include <cctype>
#include <cstdio>
#include <sstream>

namespace resq {

namespace {

const char* kCategoryNames[] = {"Medical emergency", "Fire", "Road accident", "Natural disaster", "Flood",
                                "Rescue operation",  "Other"};
const char* kSeverityNames[] = {"Critical", "High", "Medium", "Low"};
const char* kStatusNames[] = {"Pending", "Assigned", "In Progress", "Resolved", "Cancelled"};
const char* kTypeNames[] = {"Ambulance", "Medical team", "Fire engine", "Rescue team", "Relief vehicle", "Supply kit"};

}

std::string name(Category value) { return kCategoryNames[idx(value)]; }
std::string name(Severity value) { return kSeverityNames[idx(value)]; }
std::string name(Status value) { return kStatusNames[idx(value)]; }
std::string name(ResourceType value) { return kTypeNames[idx(value)]; }

namespace util {

std::string trim(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

bool icontains(const std::string& text, const std::string& part) {
    return upper(text).find(upper(part)) != std::string::npos;
}

bool toInt(const std::string& text, long long& out) {
    const std::string t = trim(text);
    if (t.empty() || t == "-" || t.size() > 12) return false;
    for (size_t i = 0; i < t.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(t[i])) && !(i == 0 && t[i] == '-')) return false;
    out = std::stoll(t);
    return true;
}

bool validId(const std::string& id) {
    if (id.size() < 3 || id.size() > 8 || !std::isalpha(static_cast<unsigned char>(id[0]))) return false;
    for (char c : id)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') return false;
    return true;
}

std::string checkText(const std::string& value, const std::string& field, size_t maxLength) {
    if (value.empty()) return field + " is required.";
    if (value.size() > maxLength) return field + " must be at most " + std::to_string(maxLength) + " characters.";
    for (unsigned char c : value)
        if (c < 32) return field + " contains invalid characters.";
    return "";
}

std::string timeText(std::time_t value) {
    const std::tm* local = value > 0 ? std::localtime(&value) : nullptr;
    char buffer[32];
    if (local == nullptr || std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", local) == 0) return "-";
    return buffer;
}

std::string duration(long long seconds) {
    if (seconds < 0) return "-";
    const long long minutes = seconds / 60;
    std::ostringstream out;
    if (minutes >= 1440)
        out << minutes / 1440 << "d " << (minutes % 1440) / 60 << "h";
    else if (minutes >= 60)
        out << minutes / 60 << "h " << minutes % 60 << "m";
    else
        out << minutes << "m";
    return out.str();
}

bool parseTime(const std::string& text, std::time_t& out) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
    char extra = 0;
    if (std::sscanf(trim(text).c_str(), "%d-%d-%d %d:%d%c", &year, &month, &day, &hour, &minute, &extra) != 5 ||
        year < 2000 || year > 2100 || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_isdst = -1;
    out = std::mktime(&tm);
    return out != static_cast<std::time_t>(-1) && tm.tm_mday == day && tm.tm_mon == month - 1;
}

std::string escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\' || c == '|') out += '\\';
        if (c != '\n' && c != '\r') out += c;
    }
    return out;
}

std::vector<std::string> split(const std::string& line, char separator) {
    std::vector<std::string> parts(1);
    for (size_t i = 0; i < line.size(); ++i)
        if (line[i] == '\\' && i + 1 < line.size())
            parts.back() += line[++i];
        else if (line[i] == separator)
            parts.emplace_back();
        else
            parts.back() += line[i];
    return parts;
}

}

}
