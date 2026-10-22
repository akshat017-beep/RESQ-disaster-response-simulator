#ifndef RESQ_CONSOLE_H
#define RESQ_CONSOLE_H

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace resq {

enum class Style {
    Plain,
    Bold,
    Heading,
    Accent,
    Critical,
    Danger,
    High,
    Warning,
    Success,
    Info,
    Progress,
    Muted,
    Highlight,
    Badge,
};

class InputClosed : public std::runtime_error {
public:
    InputClosed() : std::runtime_error("input closed") {}
};

class InputCancelled : public std::runtime_error {
public:
    InputCancelled() : std::runtime_error("cancelled by user") {}
};

struct Glyphs {
    const char* topLeft;
    const char* topRight;
    const char* bottomLeft;
    const char* bottomRight;
    const char* horizontal;
    const char* vertical;
    const char* doubleTopLeft;
    const char* doubleTopRight;
    const char* doubleBottomLeft;
    const char* doubleBottomRight;
    const char* doubleHorizontal;
    const char* doubleVertical;
    const char* block;
    const char* shade;
    const char* ellipsis;
    const char* separator;
    const char* bullet;
};

class Console {
public:
    static constexpr int kWidth = 78;

    Console(std::istream& in, std::ostream& out, bool ansi, bool color, bool unicode, bool animations,
            bool interactiveInput, bool interactiveOutput);

    bool ansi() const { return ansi_; }
    bool color() const { return color_ && ansi_; }
    void setColor(bool enabled) { color_ = enabled; }
    bool unicode() const { return unicode_; }
    void setUnicode(bool enabled) { unicode_ = enabled; }
    bool animations() const { return animations_; }
    void setAnimations(bool enabled) { animations_ = enabled; }
    bool interactiveInput() const { return interactiveInput_; }
    const Glyphs& glyphs() const;
    std::ostream& out() const { return out_; }

    std::string paint(const std::string& text, Style style) const;
    static int visibleWidth(const std::string& text);
    std::string fit(const std::string& text, int width, bool alignRight = false) const;
    static std::string repeat(const std::string& unit, int count);

    void clearScreen() const;
    void printLogo() const;
    void printHeader(const std::string& title) const;
    void printSection(const std::string& title) const;
    void printRule() const;
    std::vector<std::string> renderBox(const std::string& title, const std::vector<std::string>& lines, int width,
                                       Style borderStyle = Style::Accent) const;
    void printBox(const std::string& title, const std::vector<std::string>& lines,
                  Style borderStyle = Style::Accent) const;
    void printSideBySide(const std::vector<std::string>& left, const std::vector<std::string>& right) const;
    void printBar(const std::string& label, int value, int maxValue, Style style, int labelWidth = 18,
                  int barWidth = 40) const;
    void printMenu(const std::vector<std::pair<std::string, std::string>>& items) const;

    void success(const std::string& message) const;
    void warning(const std::string& message) const;
    void error(const std::string& message) const;
    void info(const std::string& message) const;
    void spinner(const std::string& label, int milliseconds) const;

    std::string readLine(const std::string& prompt) const;
    std::string readChoice(const std::string& prompt) const;
    std::string readText(const std::string& prompt, size_t maxLength, bool required = true) const;
    long long readNumber(const std::string& prompt, long long minimum, long long maximum) const;
    long long readNumberOr(const std::string& prompt, long long minimum, long long maximum, long long fallback) const;
    bool confirm(const std::string& prompt, bool defaultYes = false) const;
    bool confirmWord(const std::string& prompt, const std::string& word) const;
    void pause() const;

    void pushScreen(const std::string& name) { screens_.push_back(name); }
    void popScreen()
    {
        if (!screens_.empty()) {
            screens_.pop_back();
        }
    }
    std::string breadcrumb() const;

private:
    std::string prompt(const std::string& text) const;
    void message(const std::string& tag, Style style, const std::string& text) const;

    std::istream& in_;
    std::ostream& out_;
    bool ansi_;
    bool color_;
    bool unicode_;
    bool animations_;
    bool interactiveInput_;
    bool interactiveOutput_;
    std::vector<std::string> screens_;
};

class ScreenScope {
public:
    ScreenScope(Console& console, const std::string& name) : console_(console) { console_.pushScreen(name); }
    ~ScreenScope() { console_.popScreen(); }
    ScreenScope(const ScreenScope&) = delete;
    ScreenScope& operator=(const ScreenScope&) = delete;

private:
    Console& console_;
};

struct Column {
    std::string title;
    int width;
    bool alignRight = false;
};

struct Cell {
    std::string text;
    Style style = Style::Plain;
};

class Table {
public:
    explicit Table(std::vector<Column> columns, int gap = 1);
    void addRow(std::vector<Cell> cells);
    bool empty() const { return rows_.empty(); }
    void print(const Console& console) const;

private:
    std::vector<Column> columns_;
    std::vector<std::vector<Cell>> rows_;
    int gap_;
};

}

#endif
