#include "resq_app.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
    resq::AppOptions options;
    std::string error;
    if (!resq::AppOptions::parse(argc, argv, options, error)) {
        std::cerr << "resq: " << error << "\n\n" << resq::AppOptions::usage();
        return 2;
    }
    if (options.showHelp) {
        std::cout << resq::AppOptions::usage();
        return 0;
    }
    if (options.showVersion) {
        std::cout << "RESQ " << resq::kVersion << '\n';
        return 0;
    }
    try {
        resq::RESQApplication app(options);
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "RESQ stopped because of an unexpected error: " << ex.what() << '\n';
        return 1;
    }
}
