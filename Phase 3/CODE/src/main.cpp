/*
 * main.cpp  --  RESQ Phase III: Interactive Console and Entry Point
 *
 * Team member : Nitin Singh (ML-1)
 * Responsibility: Console dashboard, scenario testing, executable entry point.
 *
 * This file implements the interactive terminal UI for the RESQ simulator.
 * It is intentionally thin: all algorithm and business logic lives in the
 * controller and data-structure layers.  main.cpp only reads user input,
 * calls the appropriate ResqSystem methods, and prints results.
 *
 * Menu structure:
 *   SCENARIO
 *     1. Run fixed earthquake scenario
 *   MAP & ROUTING
 *     2. Show map
 *     3. Plan route between two locations
 *   VICTIM MANAGEMENT
 *     4. Register a new victim
 *     5. Show dashboard (all victims)
 *     6. Sort victims by priority (insertion sort)
 *     7. Sort victims by priority (quick sort)
 *     8. Search for victim by ID
 *   DISPATCH
 *     9.  Dispatch next pending victim
 *     10. Complete an operation
 *     11. Transfer victim to hospital
 *   RECORDS
 *     12. Show operation history (newest first)
 *     13. Show all operations
 *     14. List rescue units
 *     15. List hospitals
 *     16. List disasters
 *   SYSTEM
 *     17. Validate system invariants
 *     0.  Exit
 *
 * Input improvements over Phase 2 baseline:
 *   - Non-numeric input is handled with cin.clear() / retry instead of
 *     immediately exiting.
 *   - Range validation is applied to every numeric prompt.
 *   - Blank / overlong text strings are rejected with a clear message.
 */

#include "resq.hpp"

#include <iostream>
#include <limits>
#include <string>

/* ================================================================== */
/*  Input helpers                                                       */
/* ================================================================== */

/*
 * readInt -- prompt the user and read a validated integer in [min_val, max_val].
 * Retries on bad input (non-numeric or out-of-range) rather than exiting.
 * Returns false only on EOF (Ctrl+D / Ctrl+Z) so the program can exit cleanly.
 */
static bool readInt(const std::string& prompt, int& out_value,
                    int min_val, int max_val)
{
    while (true) {
        std::cout << prompt;
        if (!(std::cin >> out_value)) {
            if (std::cin.eof()) {
                return false; /* end of input stream */
            }
            std::cout << "  Please enter a whole number between "
                      << min_val << " and " << max_val << ".\n";
            std::cin.clear();
        } else if (out_value < min_val || out_value > max_val) {
            std::cout << "  Value must be between "
                      << min_val << " and " << max_val << ".\n";
        } else {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return true;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

/*
 * readText -- prompt the user and read a non-empty string up to max_chars.
 * Returns false on EOF.  Rejects empty strings and strings that exceed
 * the character limit.
 */
static bool readText(const std::string& prompt, std::string& out_text,
                     std::size_t max_chars = 79)
{
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, out_text)) {
            return false; /* EOF */
        }
        if (out_text.empty()) {
            std::cout << "  Input cannot be empty.\n";
            continue;
        }
        if (out_text.size() > max_chars) {
            std::cout << "  Input must be at most " << max_chars
                      << " characters.\n";
            continue;
        }
        return true;
    }
}

/* ================================================================== */
/*  Menu action implementations                                         */
/* ================================================================== */

static void action_register_victim(ResqSystem& sys)
{
    std::string name, location, description;
    int         affected = 1;
    int         severity = 1;

    std::cout << "\n-- Register New Victim --\n";
    if (!readText("  Name/reporter: ", name, RESQ_MAX_NAME - 1))        return;
    if (!readText("  Location: ",      location, RESQ_MAX_LOCATION - 1)) return;
    if (!readText("  Description: ",   description, RESQ_MAX_DESC - 1)) return;
    if (!readInt ("  People affected: ", affected, 1, 10000))            return;
    if (!readInt ("  Severity (1=GREEN  2=YELLOW  3=RED): ", severity, 1, 3)) return;

    int id = sys.addVictim(name, location, description, affected, severity);
    if (id < 0) {
        std::cout << "  Failed to register victim. Check inputs.\n";
    } else {
        std::cout << "  Victim registered successfully. ID: " << id << "\n";
    }
}

static void action_plan_route(ResqSystem& sys)
{
    std::string from, to;

    sys.printMap(std::cout);
    std::cout << "\n-- Plan Route --\n";
    if (!readText("  From location: ", from, RESQ_MAX_LOCATION - 1)) return;
    if (!readText("  To location:   ", to,   RESQ_MAX_LOCATION - 1)) return;

    sys.planRoute(from, to, std::cout);
}

static void action_dispatch_next(ResqSystem& sys)
{
    std::cout << "\n-- Dispatch Next Pending Victim --\n";

    int pending = sys.pendingCount();
    if (pending == 0) {
        std::cout << "  No victims are currently waiting in any queue.\n";
        return;
    }

    std::cout << "  Pending in queues: " << pending << "\n";
    bool ok = sys.dispatchNext();
    if (ok) {
        std::cout << "  Dispatch successful.\n";
    } else {
        std::cout << "  Dispatch failed: no available unit with sufficient "
                  << "capacity, or destination is unreachable.\n"
                  << "  The victim remains in the queue and will be retried.\n";
    }
}

static void action_complete_operation(ResqSystem& sys)
{
    int op_id = 0;

    sys.printOperations(std::cout);
    std::cout << "\n-- Complete an Operation --\n";
    if (!readInt("  Operation ID to complete: ", op_id, 1, 1000000)) return;

    if (sys.completeOperation(op_id)) {
        std::cout << "  Operation #" << op_id
                  << " resolved. Team released.\n";
    } else {
        std::cout << "  Could not complete operation #" << op_id
                  << ". It may not exist or may not be in-progress.\n";
    }
}

static void action_transfer_to_hospital(ResqSystem& sys)
{
    int victim_id    = 0;
    int hospital_id  = 0;
    int patient_count = 1;

    sys.printDashboard(std::cout);
    sys.listHospitals(std::cout);
    std::cout << "\n-- Transfer Victim to Hospital --\n";
    if (!readInt("  Resolved victim ID: ",       victim_id, 1, 1000000))    return;
    if (!readInt("  Hospital ID: ",              hospital_id, 1, 100000))   return;
    if (!readInt("  Number of patients to admit: ", patient_count, 1, 10000)) return;

    if (sys.transferToHospital(victim_id, hospital_id, patient_count)) {
        std::cout << "  Hospital admission recorded.\n";
    } else {
        std::cout << "  Admission failed. Victim must be resolved and "
                  << "sufficient beds must be available.\n";
    }
}

static void action_search_victim(ResqSystem& sys)
{
    int victim_id = 0;

    std::cout << "\n-- Search for Victim by ID --\n";
    if (!readInt("  Victim ID: ", victim_id, 1, 1000000)) return;
    sys.searchVictimById(std::cout, victim_id);
}

/* ================================================================== */
/*  Menu display                                                        */
/* ================================================================== */

static void printMenu()
{
    std::cout << "\n"
              << "  +----------------------------------------------------------+\n"
              << "  |           R E S Q  --  PHASE III SIMULATOR               |\n"
              << "  |      Intelligent Disaster Rescue & Emergency Response     |\n"
              << "  +----------------------------------------------------------+\n"
              << "  |  SCENARIO                                                 |\n"
              << "  |   1. Run fixed earthquake scenario                        |\n"
              << "  +----------------------------------------------------------+\n"
              << "  |  MAP & ROUTING                                            |\n"
              << "  |   2. Show disaster-area map                               |\n"
              << "  |   3. Plan route between two locations                     |\n"
              << "  +----------------------------------------------------------+\n"
              << "  |  VICTIM MANAGEMENT                                        |\n"
              << "  |   4. Register a new victim call                           |\n"
              << "  |   5. Show dashboard (all victims)                         |\n"
              << "  |   6. Sort victims by priority -- Insertion Sort           |\n"
              << "  |   7. Sort victims by priority -- Quick Sort               |\n"
              << "  |   8. Search for victim by ID                              |\n"
              << "  +----------------------------------------------------------+\n"
              << "  |  DISPATCH                                                 |\n"
              << "  |   9. Dispatch next pending victim                         |\n"
              << "  |  10. Complete an in-progress operation                    |\n"
              << "  |  11. Transfer resolved victim to hospital                 |\n"
              << "  +----------------------------------------------------------+\n"
              << "  |  RECORDS                                                  |\n"
              << "  |  12. Show operation history (newest first)                |\n"
              << "  |  13. Show all operations                                  |\n"
              << "  |  14. List rescue units                                    |\n"
              << "  |  15. List hospitals                                       |\n"
              << "  |  16. List active disasters                                |\n"
              << "  +----------------------------------------------------------+\n"
              << "  |  SYSTEM                                                   |\n"
              << "  |  17. Validate system invariants                           |\n"
              << "  |   0. Exit                                                 |\n"
              << "  +----------------------------------------------------------+\n"
              << "  Note: Educational simulation only. Not for real emergencies.\n\n"
              << "  RESQ > Enter option: ";
}

/* ================================================================== */
/*  main                                                                */
/* ================================================================== */

int main()
{
    ResqSystem sys;
    int        choice = -1;

    std::cout << "\n  Initialising RESQ Phase III Simulator...\n"
              << "  System ready. All algorithms active.\n";

    while (true) {
        printMenu();

        if (!readInt("", choice, 0, 17)) {
            break; /* EOF */
        }

        std::cout << "\n  ----------------------------------------------------------\n";

        switch (choice) {
            case 0:
                std::cout << "  RESQ session closed. Stay safe.\n";
                return 0;

            case 1:
                sys.runEarthquakeScenario(std::cout);
                break;

            case 2:
                sys.printMap(std::cout);
                break;

            case 3:
                action_plan_route(sys);
                break;

            case 4:
                action_register_victim(sys);
                break;

            case 5:
                sys.printDashboard(std::cout);
                break;

            case 6:
                sys.sortVictimsByPriority(std::cout, "insertion");
                break;

            case 7:
                sys.sortVictimsByPriority(std::cout, "quick");
                break;

            case 8:
                action_search_victim(sys);
                break;

            case 9:
                action_dispatch_next(sys);
                break;

            case 10:
                action_complete_operation(sys);
                break;

            case 11:
                action_transfer_to_hospital(sys);
                break;

            case 12:
                sys.printHistory(std::cout);
                break;

            case 13:
                sys.printOperations(std::cout);
                break;

            case 14:
                sys.listUnits(std::cout);
                break;

            case 15:
                sys.listHospitals(std::cout);
                break;

            case 16:
                sys.listDisasters(std::cout);
                break;

            case 17:
                if (sys.validateInvariants()) {
                    std::cout << "  All system invariants are valid.\n";
                } else {
                    std::cout << "  WARNING: invariant violation detected!\n";
                }
                break;

            default:
                std::cout << "  Invalid option. Please choose 0-17.\n";
                break;
        }

        std::cout << "  ----------------------------------------------------------\n";
        std::cout << "  Press Enter to return to the menu...\n";
        std::cin.get();
    }

    std::cout << "\n  Input stream closed. Exiting RESQ.\n";
    return 0;
}
