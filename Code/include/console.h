#ifndef RESQ_CONSOLE_H
#define RESQ_CONSOLE_H

#include <iostream>
#include <string>
#include <vector>

namespace resq {

enum class Color { None, Red, Green, Yellow, Cyan, Magenta, Gray, Bold };

struct InputClosed {};
struct InputCancelled {};

class Console {
public:
    Console(std::istream& in, std::ostream& out, bool color, bool interactive);

    std::ostream& out;

    std::string paint(const std::string& text, Color color) const;
    static std::string pad(const std::string& text, size_t width);
    void clear() const;
    void logo() const;
    void title(const std::string& text) const;
    void box(const std::string& heading, const std::vector<std::string>& lines, Color color = Color::Cyan) const;
    void bar(const std::string& label, int value, int maximum, Color color) const;
    void menu(const std::vector<std::string>& items) const;

    void ok(const std::string& message) const;
    void warn(const std::string& message) const;
    void error(const std::string& message) const;

    std::string line(const std::string& prompt) const;
    std::string choice(const std::string& prompt) const;
    std::string text(const std::string& prompt, size_t maxLength, bool required = true) const;
    long long number(const std::string& prompt, long long minimum, long long maximum) const;
    long long numberOr(const std::string& prompt, long long minimum, long long maximum, long long fallback) const;
    bool confirm(const std::string& prompt, bool yesByDefault = false) const;
    void pause() const;

private:
    std::istream& in_;
    bool color_;
    bool interactive_;
};

}

#endif
