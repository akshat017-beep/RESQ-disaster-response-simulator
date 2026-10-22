#include "console.h"

#include "util.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <istream>
#include <ostream>
#include <thread>

namespace resq {

namespace {

const Glyphs kUnicodeGlyphs{"┌", "┐", "└", "┘", "─", "│", "╔", "╗", "╚", "╝", "═", "║", "█", "░", "…", "›", "•"};
const Glyphs kAsciiGlyphs{"+", "+", "+", "+", "-", "|", "+", "+", "+", "+", "=", "|", "#", " ", "~", ">", "*"};

const char* ansiCode(Style style)
{
    switch (style) {
    case Style::Plain: return "";
    case Style::Bold: return "\033[1m";
    case Style::Heading: return "\033[1;96m";
    case Style::Accent: return "\033[36m";
    case Style::Critical: return "\033[1;91m";
    case Style::Danger: return "\033[91m";
    case Style::High: return "\033[1;93m";
    case Style::Warning: return "\033[33m";
    case Style::Success: return "\033[92m";
    case Style::Info: return "\033[94m";
    case Style::Progress: return "\033[95m";
    case Style::Muted: return "\033[90m";
    case Style::Highlight: return "\033[1;97m";
    case Style::Badge: return "\033[1;97;41m";
    }
    return "";
}

const char* const kLogoR[5] = {"#### ", "#   #", "#### ", "#  # ", "#   #"};
const char* const kLogoE[5] = {"#####", "#    ", "#### ", "#    ", "#####"};
const char* const kLogoS[5] = {" ####", "#    ", " ### ", "    #", "#### "};
const char* const kLogoQ[5] = {" ### ", "#   #", "#   #", "#  # ", " ## #"};

size_t utf8SequenceLength(unsigned char c)
{
    if (c < 0x80) {
        return 1;
    }
    if ((c >> 5) == 0x6) {
        return 2;
    }
    if ((c >> 4) == 0xE) {
        return 3;
    }
    if ((c >> 3) == 0x1E) {
        return 4;
    }
    return 1;
}

}

Console::Console(std::istream& in, std::ostream& out, bool ansi, bool color, bool unicode, bool animations,
                 bool interactiveInput, bool interactiveOutput)
    : in_(in),
      out_(out),
      ansi_(ansi),
      color_(color),
      unicode_(unicode),
      animations_(animations),
      interactiveInput_(interactiveInput),
      interactiveOutput_(interactiveOutput)
{
}

const Glyphs& Console::glyphs() const { return unicode_ ? kUnicodeGlyphs : kAsciiGlyphs; }

std::string Console::paint(const std::string& text, Style style) const
{
    if (!color() || style == Style::Plain || text.empty()) {
        return text;
    }
    return std::string(ansiCode(style)) + text + "\033[0m";
}

int Console::visibleWidth(const std::string& text)
{
    int width = 0;
    size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == 0x1B) {
            while (i < text.size() && text[i] != 'm') {
                ++i;
            }
            ++i;
            continue;
        }
        i += utf8SequenceLength(c);
        ++width;
    }
    return width;
}

std::string Console::fit(const std::string& text, int width, bool alignRight) const
{
    if (width <= 0) {
        return "";
    }
    std::string result = text;
    int visible = visibleWidth(text);
    if (visible > width) {
        result.clear();
        int count = 0;
        bool styled = false;
        size_t i = 0;
        while (i < text.size()) {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            if (c == 0x1B) {
                size_t j = i;
                while (j < text.size() && text[j] != 'm') {
                    ++j;
                }
                result.append(text, i, j - i + 1);
                i = j + 1;
                styled = true;
                continue;
            }
            if (count >= width - 1) {
                break;
            }
            const size_t length = std::min(utf8SequenceLength(c), text.size() - i);
            result.append(text, i, length);
            i += length;
            ++count;
        }
        result += glyphs().ellipsis;
        if (styled) {
            result += "\033[0m";
        }
        visible = width;
    }
    const std::string padding(static_cast<size_t>(width - visible), ' ');
    return alignRight ? padding + result : result + padding;
}

std::string Console::repeat(const std::string& unit, int count)
{
    std::string result;
    for (int i = 0; i < count; ++i) {
        result += unit;
    }
    return result;
}

void Console::clearScreen() const
{
    if (ansi_ && interactiveOutput_) {
        out_ << "\033[2J\033[H";
    } else {
        out_ << '\n' << repeat(glyphs().doubleHorizontal, kWidth) << '\n';
    }
}

void Console::printLogo() const
{
    const Glyphs& g = glyphs();
    const int inner = kWidth - 2;
    const auto center = [&](const std::string& text, Style style) {
        const int width = visibleWidth(text);
        const int left = std::max(0, (inner - width) / 2);
        const int right = std::max(0, inner - width - left);
        out_ << paint(g.doubleVertical, Style::Danger) << std::string(static_cast<size_t>(left), ' ')
             << paint(text, style) << std::string(static_cast<size_t>(right), ' ')
             << paint(g.doubleVertical, Style::Danger) << '\n';
    };

    out_ << paint(std::string(g.doubleTopLeft) + repeat(g.doubleHorizontal, inner) + g.doubleTopRight,
                  Style::Danger)
         << '\n';
    center("", Style::Plain);
    const char* const* letters[4] = {kLogoR, kLogoE, kLogoS, kLogoQ};
    for (int row = 0; row < 5; ++row) {
        std::string line;
        for (int letter = 0; letter < 4; ++letter) {
            if (letter > 0) {
                line += "  ";
            }
            for (const char* p = letters[letter][row]; *p != '\0'; ++p) {
                line += (*p == '#') ? std::string(g.block) + g.block : std::string("  ");
            }
        }
        center(line, Style::Critical);
    }
    center("", Style::Plain);
    center("RESCUE AND EMERGENCY SUPPORT SYSTEM", Style::Heading);
    center("Protecting Lives Through Smart Coordination", Style::Highlight);
    center("Academic prototype - not connected to any real emergency service", Style::Muted);
    out_ << paint(std::string(g.doubleBottomLeft) + repeat(g.doubleHorizontal, inner) + g.doubleBottomRight,
                  Style::Danger)
         << '\n';
}

void Console::printHeader(const std::string& title) const
{
    clearScreen();
    const Glyphs& g = glyphs();
    const std::string left = paint(" RESQ ", Style::Badge) + " " + paint("Rescue and Emergency Support System", Style::Heading);
    const std::string right = paint(util::formatDateTime(std::time(nullptr)), Style::Muted);
    const int space = std::max(1, kWidth - visibleWidth(left) - visibleWidth(right));
    out_ << left << std::string(static_cast<size_t>(space), ' ') << right << '\n';
    out_ << paint(repeat(g.doubleHorizontal, kWidth), Style::Danger) << '\n';
    const std::string crumbs = breadcrumb();
    if (!crumbs.empty()) {
        out_ << ' ' << paint(crumbs, Style::Muted) << '\n';
    }
    out_ << '\n' << ' ' << paint(util::toUpper(title), Style::Heading) << '\n';
    out_ << ' ' << paint(repeat(g.horizontal, visibleWidth(title)), Style::Accent) << "\n\n";
}

void Console::printSection(const std::string& title) const
{
    const Glyphs& g = glyphs();
    const std::string label = " " + util::toUpper(title) + " ";
    const int remaining = std::max(2, kWidth - 4 - visibleWidth(label));
    out_ << '\n'
         << ' ' << paint(repeat(g.horizontal, 2), Style::Accent) << paint(label, Style::Heading)
         << paint(repeat(g.horizontal, remaining), Style::Accent) << '\n';
}

void Console::printRule() const { out_ << ' ' << paint(repeat(glyphs().horizontal, kWidth - 2), Style::Muted) << '\n'; }

std::vector<std::string> Console::renderBox(const std::string& title, const std::vector<std::string>& lines,
                                            int width, Style borderStyle) const
{
    const Glyphs& g = glyphs();
    std::vector<std::string> result;
    const int inner = width - 4;
    if (title.empty()) {
        result.push_back(paint(std::string(g.topLeft) + repeat(g.horizontal, width - 2) + g.topRight, borderStyle));
    } else {
        const std::string label = fit(" " + title + " ", std::min(width - 4, visibleWidth(title) + 2));
        const int rest = width - 3 - visibleWidth(label);
        result.push_back(paint(std::string(g.topLeft) + g.horizontal, borderStyle) + paint(label, Style::Highlight) +
                         paint(repeat(g.horizontal, rest) + g.topRight, borderStyle));
    }
    for (const std::string& line : lines) {
        result.push_back(paint(g.vertical, borderStyle) + " " + fit(line, inner) + " " +
                         paint(g.vertical, borderStyle));
    }
    result.push_back(paint(std::string(g.bottomLeft) + repeat(g.horizontal, width - 2) + g.bottomRight, borderStyle));
    return result;
}

void Console::printBox(const std::string& title, const std::vector<std::string>& lines, Style borderStyle) const
{
    for (const std::string& line : renderBox(title, lines, kWidth, borderStyle)) {
        out_ << line << '\n';
    }
}

void Console::printSideBySide(const std::vector<std::string>& left, const std::vector<std::string>& right) const
{
    int leftWidth = 0;
    for (const std::string& line : left) {
        leftWidth = std::max(leftWidth, visibleWidth(line));
    }
    const size_t rows = std::max(left.size(), right.size());
    for (size_t i = 0; i < rows; ++i) {
        const std::string l = i < left.size() ? left[i] : "";
        const std::string r = i < right.size() ? right[i] : "";
        out_ << fit(l, leftWidth) << "  " << r << '\n';
    }
}

void Console::printBar(const std::string& label, int value, int maxValue, Style style, int labelWidth,
                       int barWidth) const
{
    const Glyphs& g = glyphs();
    int filled = 0;
    if (maxValue > 0 && value > 0) {
        filled = static_cast<int>(std::lround(static_cast<double>(value) * barWidth / maxValue));
        filled = std::max(1, std::min(filled, barWidth));
    }
    out_ << "  " << fit(label, labelWidth) << ' ' << paint(g.vertical, Style::Muted) << ' '
         << paint(repeat(g.block, filled), style) << paint(repeat(g.shade, barWidth - filled), Style::Muted) << ' '
         << paint(std::to_string(value), Style::Highlight) << '\n';
}

void Console::printMenu(const std::vector<std::pair<std::string, std::string>>& items) const
{
    const size_t half = (items.size() + 1) / 2;
    const auto render = [&](const std::pair<std::string, std::string>& item) {
        return paint("[" + item.first + "]", Style::Heading) + " " + item.second;
    };
    for (size_t i = 0; i < half; ++i) {
        std::string line = "  " + fit(render(items[i]), 36);
        if (i + half < items.size()) {
            line += "  " + render(items[i + half]);
        }
        out_ << line << '\n';
    }
}

void Console::message(const std::string& tag, Style style, const std::string& text) const
{
    out_ << "  " << paint(tag, style) << ' ' << text << '\n';
}

void Console::success(const std::string& text) const { message("[ OK ]", Style::Success, text); }
void Console::warning(const std::string& text) const { message("[WARN]", Style::Warning, text); }
void Console::error(const std::string& text) const { message("[FAIL]", Style::Danger, text); }
void Console::info(const std::string& text) const { message("[INFO]", Style::Info, text); }

void Console::spinner(const std::string& label, int milliseconds) const
{
    if (!animations_ || !ansi_ || !interactiveOutput_) {
        return;
    }
    static const char* const unicodeFrames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static const char* const asciiFrames[] = {"|", "/", "-", "\\"};
    const char* const* frames = unicode_ ? unicodeFrames : asciiFrames;
    const int frameCount = unicode_ ? 10 : 4;
    const int steps = std::max(1, milliseconds / 70);
    for (int i = 0; i < steps; ++i) {
        out_ << "\r  " << paint(frames[i % frameCount], Style::Accent) << ' ' << label << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
    }
    out_ << "\r\033[2K" << std::flush;
}

std::string Console::prompt(const std::string& text) const
{
    return "  " + paint(text, Style::Bold) + paint(std::string(" ") + glyphs().separator + " ", Style::Accent);
}

std::string Console::readLine(const std::string& text) const
{
    out_ << prompt(text) << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        out_ << '\n';
        throw InputClosed();
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (!interactiveInput_) {
        out_ << line << '\n';
    }
    return line;
}

std::string Console::readChoice(const std::string& text) const
{
    std::string choice = util::toUpper(util::trim(readLine(text)));
    if (choice == ":Q") {
        choice = "0";
    }
    return choice;
}

std::string Console::readText(const std::string& text, size_t maxLength, bool required) const
{
    while (true) {
        const std::string value = util::trim(readLine(text));
        if (value == ":q" || value == ":Q") {
            throw InputCancelled();
        }
        if (value.empty()) {
            if (!required) {
                return value;
            }
            error("This field is required (type :q to cancel).");
            continue;
        }
        if (util::utf8Length(value) > maxLength) {
            error("Too long: at most " + std::to_string(maxLength) + " characters are allowed.");
            continue;
        }
        if (!util::isPrintableText(value)) {
            error("Control characters are not allowed.");
            continue;
        }
        return value;
    }
}

long long Console::readNumber(const std::string& text, long long minimum, long long maximum) const
{
    while (true) {
        const std::string value = util::trim(readLine(text));
        if (value == ":q" || value == ":Q") {
            throw InputCancelled();
        }
        long long number = 0;
        if (!util::parseInteger(value, number) || number < minimum || number > maximum) {
            error("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) +
                  " (or :q to cancel).");
            continue;
        }
        return number;
    }
}

long long Console::readNumberOr(const std::string& text, long long minimum, long long maximum,
                                long long fallback) const
{
    while (true) {
        const std::string value = util::trim(readLine(text + " [" + std::to_string(fallback) + "]"));
        if (value.empty()) {
            return fallback;
        }
        if (value == ":q" || value == ":Q") {
            throw InputCancelled();
        }
        long long number = 0;
        if (!util::parseInteger(value, number) || number < minimum || number > maximum) {
            error("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) +
                  " (Enter keeps " + std::to_string(fallback) + ").");
            continue;
        }
        return number;
    }
}

bool Console::confirm(const std::string& text, bool defaultYes) const
{
    while (true) {
        const std::string value = util::toLower(util::trim(readLine(text + (defaultYes ? " (Y/n)" : " (y/N)"))));
        if (value.empty()) {
            return defaultYes;
        }
        if (value == "y" || value == "yes") {
            return true;
        }
        if (value == "n" || value == "no" || value == ":q") {
            return false;
        }
        error("Please answer y or n.");
    }
}

bool Console::confirmWord(const std::string& text, const std::string& word) const
{
    const std::string value = util::trim(readLine(text + " - type " + word + " to confirm"));
    return value == word;
}

void Console::pause() const
{
    if (!interactiveInput_) {
        return;
    }
    out_ << '\n';
    readLine(paint("Press Enter to continue", Style::Muted));
}

std::string Console::breadcrumb() const
{
    std::string result;
    for (size_t i = 0; i < screens_.size(); ++i) {
        if (i > 0) {
            result += std::string(" ") + glyphs().separator + " ";
        }
        result += screens_[i];
    }
    return result;
}

Table::Table(std::vector<Column> columns, int gap) : columns_(std::move(columns)), gap_(gap) {}

void Table::addRow(std::vector<Cell> cells)
{
    cells.resize(columns_.size());
    rows_.push_back(std::move(cells));
}

void Table::print(const Console& console) const
{
    std::ostream& out = console.out();
    const std::string gap(static_cast<size_t>(gap_), ' ');
    std::string header = " ";
    std::string rule = " ";
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0) {
            header += gap;
            rule += gap;
        }
        header += console.paint(console.fit(columns_[i].title, columns_[i].width, columns_[i].alignRight),
                                Style::Heading);
        rule += Console::repeat(console.glyphs().horizontal, columns_[i].width);
    }
    out << header << '\n' << console.paint(rule, Style::Muted) << '\n';
    if (rows_.empty()) {
        out << "  " << console.paint("(no records)", Style::Muted) << '\n';
        return;
    }
    for (const auto& row : rows_) {
        std::string line = " ";
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) {
                line += gap;
            }
            line += console.paint(console.fit(row[i].text, columns_[i].width, columns_[i].alignRight), row[i].style);
        }
        out << line << '\n';
    }
}

}
