#include "app.h"

#include "resq_c.h"

int main(int argc, char* argv[]) {
    std::string folder = "data";
    bool color = resq_enable_ansi() == 1;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-color") {
            color = false;
        } else if (arg == "--help") {
            std::cout << "Usage: resq [--no-color] [data-folder]\n";
            return 0;
        } else {
            folder = arg;
        }
    }
    resq::RESQApplication app(folder, color);
    return app.run();
}
