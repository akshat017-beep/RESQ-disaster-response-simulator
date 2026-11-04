#include "rescue_system.h"
#include "resq_c.h"
#include <iostream>
#include <string>
#include <limits>
#include <vector>
#include <cstdlib>

static bool readInt(const std::string& prompt, int& value, int low, int high) {
    while (true) {
        std::cout << prompt;
        if (std::cin >> value) {
            if (value >= low && value <= high) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return true;
            }
            std::cout << "Enter a value from " << low << " to " << high << ".\n";
        } else {
            if (std::cin.eof()) return false;
            std::cout << "Enter a valid whole number.\n";
            std::cin.clear();
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

static bool readText(const std::string& prompt, std::string& value, std::size_t limit = 79) {
    std::cout << prompt;
    std::getline(std::cin, value);
    if (std::cin.eof()) return false;
    if (value.empty() || value.size() > limit) {
        std::cout << "Text must contain 1 to " << limit << " characters.\n";
        return false;
    }
    return true;
}

static int severityInput() {
    int severity = 1;
    std::cout << "Severity: 1 GREEN, 2 YELLOW, 3 RED\n";
    readInt("Choose severity: ", severity, 1, 3);
    return severity;
}

static void addVictim(ResqScenarioManager& app) {
    std::string name, location, description;
    int affected = 1;
    if (!readText("Victim/reporter's name: ", name)) return;
    if (!readText("Location: ", location)) return;
    if (!readText("Short description: ", description, 159)) return;
    if (!readInt("Number of people affected: ", affected, 1, 10000)) return;
    int severity = severityInput();
    int id = app.addVictim(name, location, description, affected, severity);
    if (id < 0) std::cout << "Victim could not be registered. Check the entered values.\n";
    else std::cout << "Victim registered. ID: " << id << "\n";
}

static void addAmbulance(ResqScenarioManager& app) {
    std::string name, location;
    int capacity = 1;
    if (!readText("Ambulance name: ", name)) return;
    if (!readText("Current location: ", location)) return;
    if (!readInt("Patient capacity: ", capacity, 1, 10000)) return;
    int id = app.addAmbulance(name, location, capacity);
    if (id < 0) std::cout << "Ambulance could not be added.\n";
    else std::cout << "Ambulance added. Team ID: " << id << "\n";
}

static void addCrew(ResqScenarioManager& app) {
    std::string name, location;
    int members = 1;
    if (!readText("Rescue crew name: ", name)) return;
    if (!readText("Base location: ", location)) return;
    if (!readInt("Number of crew members: ", members, 1, 10000)) return;
    int id = app.addRescueCrew(name, location, members);
    if (id < 0) std::cout << "Rescue crew could not be added.\n";
    else std::cout << "Rescue crew added. Team ID: " << id << "\n";
}

static void addHospital(ResqScenarioManager& app) {
    std::string name, location;
    int beds = 1;
    if (!readText("Hospital name: ", name)) return;
    if (!readText("Hospital location: ", location)) return;
    if (!readInt("Total beds: ", beds, 1, 100000)) return;
    int id = app.addHospital(name, location, beds);
    if (id < 0) std::cout << "Hospital could not be added.\n";
    else std::cout << "Hospital added. ID: " << id << "\n";
}

static void addDisaster(ResqScenarioManager& app) {
    std::string type, area;
    int affected = 0;
    if (!readText("Disaster type (flood/fire/earthquake/etc.): ", type)) return;
    if (!readText("Affected area: ", area)) return;
    int severity = severityInput();
    if (!readInt("Estimated people affected: ", affected, 0, 1000000)) return;
    int id = app.addDisaster(type, area, severity, affected);
    if (id < 0) std::cout << "Disaster record could not be added.\n";
    else std::cout << "Disaster recorded. ID: " << id << "\n";
}

static void dispatchVictim(ResqScenarioManager& app) {
    int victimId = 0, teamId = 0, cost = 0;
    std::string route;
    app.listVictims();
    if (!readInt("Victim ID to dispatch: ", victimId, 1, 100000000)) return;
    app.listTeams();
    if (!readInt("Rescue team ID: ", teamId, 1, 100000000)) return;
    if (!readText("Route description (e.g. CMD -> Market): ", route)) return;
    if (!readInt("Route cost: ", cost, 0, 1000000)) return;
    if (app.dispatchVictim(victimId, teamId, route, cost))
        std::cout << "Rescue operation created. Use Start Operation to begin.\n";
    else
        std::cout << "Dispatch failed. Check victim status, team availability and capacity.\n";
}

static void startOperation(ResqScenarioManager& app) {
    int id = 0;
    app.listOperations();
    if (!readInt("Operation ID to start: ", id, 1, 100000000)) return;
    if (app.startOperation(id)) std::cout << "Operation started.\n";
    else std::cout << "Operation could not be started.\n";
}

static void completeOperation(ResqScenarioManager& app) {
    int id = 0;
    app.listOperations();
    if (!readInt("Operation ID to complete: ", id, 1, 100000000)) return;
    if (app.completeOperation(id)) std::cout << "Operation resolved and team released.\n";
    else std::cout << "Operation could not be completed. Start it first.\n";
}

static void cancelOperation(ResqScenarioManager& app) {
    int id = 0;
    app.listOperations();
    if (!readInt("Operation ID to cancel: ", id, 1, 100000000)) return;
    if (app.cancelOperation(id)) std::cout << "Operation cancelled and team released.\n";
    else std::cout << "Operation could not be cancelled.\n";
}

static void admitPatients(ResqScenarioManager& app) {
    int victimId = 0, hospitalId = 0, people = 1;
    app.listVictims();
    if (!readInt("Resolved victim ID: ", victimId, 1, 100000000)) return;
    app.listHospitals();
    if (!readInt("Hospital ID: ", hospitalId, 1, 100000000)) return;
    if (!readInt("Number of people to admit: ", people, 1, 100000)) return;
    if (app.admitVictim(victimId, hospitalId, people)) std::cout << "Hospital admission recorded.\n";
    else std::cout << "Admission failed. Victim must be resolved and beds must be available.\n";
}

static void addLocation(ResqScenarioManager& app) {
    std::string name;
    if (!readText("Location name: ", name)) return;
    int id = app.addLocation(name);
    if (id < 0) std::cout << "Location could not be added.\n";
    else std::cout << "Location added with ID " << id << ".\n";
}

static void addRoad(ResqScenarioManager& app) {
    int from = 0, to = 0, cost = 1;
    app.listLocations();
    if (!readInt("From location number: ", from, 1, 10000)) return;
    if (!readInt("To location number: ", to, 1, 10000)) return;
    if (!readInt("Positive road cost: ", cost, 1, 100000)) return;
    if (app.addRoad(from - 1, to - 1, cost)) std::cout << "Road input accepted by the scenario manager.\n";
    else std::cout << "Road could not be added. Check location numbers and cost.\n";
}

static void showMenu() {
    std::cout << "\n";
    std::cout << "  +------------------------------------------------------------------+\n";
    std::cout << "  |                 R E S Q   /   P H A S E   I I                    |\n";
    std::cout << "  |              DISASTER RESPONSE SIMULATION SYSTEM                |\n";
    std::cout << "  +------------------------------------------------------------------+\n";
    std::cout << "  |  RESCUE OPERATIONS                 |  RECORDS & OVERVIEW          |\n";
    std::cout << "  |  01  Register victim               |  13  List victims            |\n";
    std::cout << "  |  02  Add ambulance                 |  14  List rescue teams       |\n";
    std::cout << "  |  03  Add rescue crew                |  15  List hospitals          |\n";
    std::cout << "  |  04  Add hospital                   |  16  List disasters          |\n";
    std::cout << "  |  05  Register disaster              |  17  List locations          |\n";
    std::cout << "  |  06  Add location                   |  18  List operations         |\n";
    std::cout << "  |  07  Add road                       |  19  Show system summary     |\n";
    std::cout << "  |  08  Dispatch victim                |  20  Run scenario tests      |\n";
    std::cout << "  |  09  Start operation                |                              |\n";
    std::cout << "  |  10  Complete operation             |                              |\n";
    std::cout << "  |  11  Cancel operation               |                              |\n";
    std::cout << "  |  12  Admit patient to hospital      |                              |\n";
    std::cout << "  +------------------------------------------------------------------+\n";
    std::cout << "  |  00  Exit simulator                                               |\n";
    std::cout << "  +------------------------------------------------------------------+\n";
    std::cout << "  Note: Educational simulation only; not for real emergency dispatch.\n\n";
}

int main() {
    ResqScenarioManager app;
    int choice = -1;
    std::cout << "\n  Initializing RESQ response console...\n";
    std::cout << "  System ready. Select an action from the menu below.\n";
    while (true) {
        showMenu();
        if (!readInt("  RESQ > Enter option: ", choice, 0, 20)) break;
        std::cout << "\n  --------------------------------------------------------------------\n";
        switch (choice) {
            case 1: addVictim(app); break;
            case 2: addAmbulance(app); break;
            case 3: addCrew(app); break;
            case 4: addHospital(app); break;
            case 5: addDisaster(app); break;
            case 6: addLocation(app); break;
            case 7: addRoad(app); break;
            case 8: dispatchVictim(app); break;
            case 9: startOperation(app); break;
            case 10: completeOperation(app); break;
            case 11: cancelOperation(app); break;
            case 12: admitPatients(app); break;
            case 13: app.listVictims(); break;
            case 14: app.listTeams(); break;
            case 15: app.listHospitals(); break;
            case 16: app.listDisasters(); break;
            case 17: app.listLocations(); break;
            case 18: app.listOperations(); break;
            case 19: app.showSummary(); break;
            case 20: app.runScenarioTests(); break;
            case 0: std::cout << "\n  RESQ session closed. Stay safe.\n"; return 0;
            default: std::cout << "  Invalid menu option.\n"; break;
        }
        if (choice != 0) {
            std::cout << "  --------------------------------------------------------------------\n";
            std::cout << "  Press Enter to return to the main menu...";
            std::cin.get();
        }
    }
    std::cout << "\n  Input closed. Exiting RESQ.\n";
    return 0;
}
