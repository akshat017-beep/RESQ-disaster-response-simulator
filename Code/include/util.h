#ifndef RESQ_UTIL_H
#define RESQ_UTIL_H

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace resq {
namespace util {

std::string trim(const std::string& text);
std::string toUpper(std::string text);
std::string toLower(std::string text);
bool iequals(const std::string& a, const std::string& b);
bool icontains(const std::string& haystack, const std::string& needle);

size_t utf8Length(const std::string& text);
size_t utf8Offset(const std::string& text, size_t chars);

bool isPrintableText(const std::string& text);

bool isValidId(const std::string& id);

bool parseInteger(const std::string& text, long long& out);

std::string escapeField(const std::string& field);
std::vector<std::string> splitEscaped(const std::string& line);
std::vector<std::string> splitPlain(const std::string& text, char delimiter);

std::vector<std::string> wrapText(const std::string& text, size_t width);

std::string formatDateTime(std::time_t value);
std::string formatCompact(std::time_t value);
std::string formatClock(std::time_t value);
std::string formatDuration(long long seconds);
bool parseDateTime(const std::string& text, std::time_t& out);

std::string obfuscate(const std::string& plain);
bool reveal(const std::string& stored, std::string& plain);
std::string maskContact(const std::string& contact);

std::string hex32(std::uint32_t value);

}
}

#endif
