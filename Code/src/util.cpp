#include "util.h"

#include "resq_c.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace resq {
namespace util {

std::string trim(const std::string& text)
{
    const char* spaces = " \t\r\n\v\f";
    const size_t first = text.find_first_not_of(spaces);
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = text.find_last_not_of(spaces);
    return text.substr(first, last - first + 1);
}

std::string toUpper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool iequals(const std::string& a, const std::string& b)
{
    return toLower(a) == toLower(b);
}

bool icontains(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) {
        return true;
    }
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

size_t utf8Length(const std::string& text)
{
    size_t count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

size_t utf8Offset(const std::string& text, size_t chars)
{
    size_t seen = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0xC0) != 0x80) {
            if (seen == chars) {
                return i;
            }
            ++seen;
        }
    }
    return text.size();
}

bool isPrintableText(const std::string& text)
{
    for (unsigned char c : text) {
        if (c < 0x20 || c == 0x7F) {
            return false;
        }
    }
    return true;
}

bool isValidId(const std::string& id)
{
    if (id.size() < 3 || id.size() > 8) {
        return false;
    }
    if (!std::isupper(static_cast<unsigned char>(id[0]))) {
        return false;
    }
    for (unsigned char c : id) {
        if (!(std::isupper(c) || std::isdigit(c) || c == '-')) {
            return false;
        }
    }
    return true;
}

bool parseInteger(const std::string& text, long long& out)
{
    const std::string t = trim(text);
    if (t.empty()) {
        return false;
    }
    size_t start = (t[0] == '+' || t[0] == '-') ? 1 : 0;
    if (start == t.size() || t.size() - start > 18) {
        return false;
    }
    for (size_t i = start; i < t.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(t[i]))) {
            return false;
        }
    }
    out = std::stoll(t);
    return true;
}

std::string escapeField(const std::string& field)
{
    std::string out;
    out.reserve(field.size());
    for (char c : field) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '|': out += "\\p"; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        default: out += c; break;
        }
    }
    return out;
}

std::vector<std::string> splitEscaped(const std::string& line)
{
    std::vector<std::string> fields;
    std::string current;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '\\' && i + 1 < line.size()) {
            const char next = line[++i];
            if (next == 'p') {
                current += '|';
            } else if (next == 'n') {
                current += '\n';
            } else {
                current += next;
            }
        } else if (c == '|') {
            fields.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    fields.push_back(current);
    return fields;
}

std::vector<std::string> splitPlain(const std::string& text, char delimiter)
{
    std::vector<std::string> parts;
    std::string current;
    std::istringstream in(text);
    while (std::getline(in, current, delimiter)) {
        current = trim(current);
        if (!current.empty()) {
            parts.push_back(current);
        }
    }
    return parts;
}

std::vector<std::string> wrapText(const std::string& text, size_t width)
{
    std::vector<std::string> lines;
    if (width == 0) {
        lines.push_back(text);
        return lines;
    }
    std::istringstream in(text);
    std::string word;
    std::string current;
    while (in >> word) {
        while (utf8Length(word) > width) {
            if (!current.empty()) {
                lines.push_back(current);
                current.clear();
            }
            const size_t cut = utf8Offset(word, width);
            lines.push_back(word.substr(0, cut));
            word = word.substr(cut);
        }
        if (word.empty()) {
            continue;
        }
        if (current.empty()) {
            current = word;
        } else if (utf8Length(current) + 1 + utf8Length(word) <= width) {
            current += " " + word;
        } else {
            lines.push_back(current);
            current = word;
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}

namespace {

std::string formatWith(std::time_t value, const char* pattern)
{
    if (value <= 0) {
        return "-";
    }
    const std::tm* local = std::localtime(&value);
    if (local == nullptr) {
        return "-";
    }
    char buffer[64];
    if (std::strftime(buffer, sizeof(buffer), pattern, local) == 0) {
        return "-";
    }
    return buffer;
}

}

std::string formatDateTime(std::time_t value) { return formatWith(value, "%Y-%m-%d %H:%M"); }
std::string formatCompact(std::time_t value) { return formatWith(value, "%m-%d %H:%M"); }
std::string formatClock(std::time_t value) { return formatWith(value, "%H:%M"); }

std::string formatDuration(long long seconds)
{
    if (seconds < 0) {
        return "-";
    }
    if (seconds < 60) {
        return "<1m";
    }
    const long long minutes = seconds / 60;
    const long long days = minutes / (24 * 60);
    const long long hours = (minutes / 60) % 24;
    const long long mins = minutes % 60;
    std::ostringstream out;
    if (days > 0) {
        out << days << "d " << std::setw(2) << std::setfill('0') << hours << "h";
    } else if (hours > 0) {
        out << hours << "h " << std::setw(2) << std::setfill('0') << mins << "m";
    } else {
        out << mins << "m";
    }
    return out.str();
}

bool parseDateTime(const std::string& text, std::time_t& out)
{
    const std::string t = trim(text);
    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
    char extra = '\0';
    if (std::sscanf(t.c_str(), "%4d-%2d-%2d %2d:%2d%c", &year, &month, &day, &hour, &minute, &extra) != 5) {
        return false;
    }
    if (year < 1971 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59) {
        return false;
    }
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_isdst = -1;
    const std::time_t result = std::mktime(&tm);
    if (result == static_cast<std::time_t>(-1)) {
        return false;
    }
    if (tm.tm_mday != day || tm.tm_mon != month - 1 || tm.tm_year != year - 1900) {
        return false;
    }
    out = result;
    return true;
}

std::string obfuscate(const std::string& plain)
{
    std::vector<char> buffer(plain.size() * 2 + 1, '\0');
    resq_obfuscate_hex(plain.c_str(), buffer.data(), buffer.size());
    return "X1:" + std::string(buffer.data());
}

bool reveal(const std::string& stored, std::string& plain)
{
    if (stored.rfind("X1:", 0) != 0) {
        plain = stored;
        return true;
    }
    const std::string hex = stored.substr(3);
    std::vector<char> buffer(hex.size() / 2 + 1, '\0');
    if (!resq_deobfuscate_hex(hex.c_str(), buffer.data(), buffer.size())) {
        return false;
    }
    plain.assign(buffer.data());
    return true;
}

std::string maskContact(const std::string& contact)
{
    const size_t length = utf8Length(contact);
    if (length <= 3) {
        return std::string(length, '*');
    }
    const size_t keepFrom = utf8Offset(contact, length - 3);
    return std::string(length - 3, '*') + contact.substr(keepFrom);
}

std::string hex32(std::uint32_t value)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08X", static_cast<unsigned int>(value));
    return buffer;
}

}
}
