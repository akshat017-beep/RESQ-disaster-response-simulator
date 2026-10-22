#include "console.h"

#include "types.h"

#include <algorithm>

namespace resq {

namespace {

const int kWidth = 74;

size_t visible(const std::string& text) {
    size_t count = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\033') {
            i = text.find('m', i);
            if (i == std::string::npos) break;
        } else {
            ++count;
        }
    }
    return count;
}

}

Console::Console(std::istream& in, std::ostream& output, bool color, bool interactive)
    : out(output), in_(in), color_(color), interactive_(interactive) {}

std::string Console::paint(const std::string& text, Color color) const {
    static const char* codes[] = {"",         "\033[1;91m", "\033[92m", "\033[93m",
                                  "\033[96m", "\033[95m",   "\033[90m", "\033[1m"};
    if (!color_ || color == Color::None) return text;
    return codes[static_cast<int>(color)] + text + "\033[0m";
}

std::string Console::pad(const std::string& text, size_t width) {
    if (text.size() > width) return text.substr(0, width - 1) + "~";
    return text + std::string(width - text.size(), ' ');
}

void Console::clear() const { out << (color_ && interactive_ ? "\033[2J\033[H" : "\n") << std::flush; }

void Console::logo() const {
    static const char* letters[4][5] = {
        {"#### ", "#   #", "#### ", "#  # ", "#   #"},
        {"#####", "#    ", "#### ", "#    ", "#####"},
        {" ####", "#    ", " ### ", "    #", "#### "},
        {" ### ", "#   #", "#   #", "#  # ", " ## #"},
    };
    const std::string border = "+" + std::string(kWidth, '=') + "+";
    out << paint(border, Color::Red) << '\n';
    for (int row = 0; row < 5; ++row) {
        std::string text;
        for (int letter = 0; letter < 4; ++letter) {
            for (const char* p = letters[letter][row]; *p != '\0'; ++p) text += *p == '#' ? "##" : "  ";
            text += "  ";
        }
        out << paint("|", Color::Red) << std::string(14, ' ') << paint(pad(text, kWidth - 14), Color::Red)
            << paint("|", Color::Red) << '\n';
    }
    out << paint("|", Color::Red)
        << paint(pad("          RESQ - Rescue and Emergency Support System", kWidth), Color::Cyan)
        << paint("|", Color::Red) << '\n'
        << paint("|", Color::Red)
        << paint(pad("          Academic prototype - not connected to real emergency services", kWidth), Color::Gray)
        << paint("|", Color::Red) << '\n'
        << paint(border, Color::Red) << '\n';
}

void Console::title(const std::string& text) const {
    clear();
    out << paint(" RESQ ", Color::Red) << "| " << paint(util::upper(text), Color::Cyan) << '\n'
        << paint(std::string(kWidth + 2, '-'), Color::Gray) << '\n';
}

void Console::box(const std::string& heading, const std::vector<std::string>& lines, Color color) const {
    out << paint("+-- ", color) << paint(heading, Color::Bold) << ' '
        << paint(std::string(kWidth - 4 - heading.size(), '-') + "+", color) << '\n';
    for (const std::string& line : lines) {
        const size_t width = visible(line);
        out << paint("| ", color) << line << std::string(width < kWidth - 2 ? kWidth - 2 - width : 0, ' ')
            << paint(" |", color) << '\n';
    }
    out << paint("+" + std::string(kWidth, '-') + "+", color) << '\n';
}

void Console::bar(const std::string& label, int value, int maximum, Color color) const {
    const int width = maximum > 0 ? std::max(value > 0 ? 1 : 0, value * 40 / maximum) : 0;
    out << "  " << pad(label, 18) << "| " << paint(std::string(static_cast<size_t>(width), '#'), color)
        << std::string(static_cast<size_t>(41 - width), ' ') << value << '\n';
}

void Console::menu(const std::vector<std::string>& items) const {
    for (size_t i = 0; i < items.size(); i += 2)
        out << "  " << pad(items[i], 36) << (i + 1 < items.size() ? items[i + 1] : "") << '\n';
}

void Console::ok(const std::string& message) const {
    out << "  " << paint("[ OK ]", Color::Green) << ' ' << message << '\n';
}
void Console::warn(const std::string& message) const {
    out << "  " << paint("[WARN]", Color::Yellow) << ' ' << message << '\n';
}
void Console::error(const std::string& message) const {
    out << "  " << paint("[FAIL]", Color::Red) << ' ' << message << '\n';
}

std::string Console::line(const std::string& prompt) const {
    out << "  " << paint(prompt, Color::Bold) << paint(" > ", Color::Cyan) << std::flush;
    std::string value;
    if (!std::getline(in_, value)) {
        out << '\n';
        throw InputClosed();
    }
    if (!interactive_) out << value << '\n';
    return util::trim(value);
}

std::string Console::choice(const std::string& prompt) const { return util::upper(line(prompt)); }

std::string Console::text(const std::string& prompt, size_t maxLength, bool required) const {
    while (true) {
        const std::string value = line(prompt);
        if (value == ":q") throw InputCancelled();
        if (value.empty() && !required) return value;
        const std::string problem = util::checkText(value, "This field", maxLength);
        if (problem.empty()) return value;
        error(problem + " (type :q to cancel)");
    }
}

long long Console::number(const std::string& prompt, long long minimum, long long maximum) const {
    while (true) {
        const std::string value = line(prompt);
        long long result = 0;
        if (value == ":q") throw InputCancelled();
        if (util::toInt(value, result) && result >= minimum && result <= maximum) return result;
        error("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) + ".");
    }
}

long long Console::numberOr(const std::string& prompt, long long minimum, long long maximum, long long fallback) const {
    while (true) {
        const std::string value = line(prompt + " [" + std::to_string(fallback) + "]");
        long long result = 0;
        if (value.empty()) return fallback;
        if (util::toInt(value, result) && result >= minimum && result <= maximum) return result;
        error("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) + ".");
    }
}

bool Console::confirm(const std::string& prompt, bool yesByDefault) const {
    while (true) {
        const std::string value = util::upper(line(prompt + (yesByDefault ? " (Y/n)" : " (y/N)")));
        if (value.empty()) return yesByDefault;
        if (value == "Y" || value == "YES") return true;
        if (value == "N" || value == "NO") return false;
        error("Please answer y or n.");
    }
}

void Console::pause() const {
    if (interactive_) line("Press Enter to continue");
}

}
