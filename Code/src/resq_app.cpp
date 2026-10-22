#include "resq_app.h"

#include "demo_data.h"
#include "priority.h"
#include "resq_c.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

namespace resq {

namespace {

using Row = std::pair<std::string, std::pair<std::string, Style>>;

Style severityStyle(Severity severity)
{
    switch (severity) {
    case Severity::Critical: return Style::Critical;
    case Severity::High: return Style::High;
    case Severity::Medium: return Style::Warning;
    case Severity::Low: return Style::Plain;
    }
    return Style::Plain;
}

Style statusStyle(Status status)
{
    switch (status) {
    case Status::Pending: return Style::Warning;
    case Status::Assigned: return Style::Accent;
    case Status::InProgress: return Style::Progress;
    case Status::Resolved: return Style::Success;
    case Status::Cancelled: return Style::Muted;
    }
    return Style::Plain;
}

Style resourceStyle(ResourceStatus status)
{
    switch (status) {
    case ResourceStatus::Available: return Style::Success;
    case ResourceStatus::Deployed: return Style::Warning;
    case ResourceStatus::OutOfService: return Style::Danger;
    }
    return Style::Plain;
}

int defaultCapacity(ResourceType type)
{
    switch (type) {
    case ResourceType::Ambulance: return 2;
    case ResourceType::MedicalTeam: return 10;
    case ResourceType::FireEngine: return 6;
    case ResourceType::RescueTeam: return 8;
    case ResourceType::ReliefVehicle: return 20;
    case ResourceType::SupplyKit: return 25;
    }
    return 0;
}

std::string statusHint(Status status)
{
    switch (status) {
    case Status::Pending: return "Return to the queue (releases all resources)";
    case Status::Assigned: return "Assign a resource now";
    case Status::InProgress: return "Crews have started work on scene";
    case Status::Resolved: return "Operation complete (releases all resources)";
    case Status::Cancelled: return "False alarm / duplicate / not needed (releases resources)";
    }
    return "";
}

std::string percent(int part, int whole)
{
    if (whole <= 0) {
        return "0%";
    }
    return std::to_string(static_cast<int>(std::lround(100.0 * part / whole))) + "%";
}

std::string resourceList(const std::vector<const Resource*>& list)
{
    std::string text;
    for (const Resource* r : list) {
        text += (text.empty() ? "" : ", ") + r->id() + " (" + shortName(r->type()) + ")";
    }
    return text.empty() ? "none" : text;
}

}

bool AppOptions::parse(int argc, char* argv[], AppOptions& options, std::string& error)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--data-dir") {
            if (i + 1 >= argc) {
                error = "--data-dir needs a directory name.";
                return false;
            }
            options.dataDirectory = argv[++i];
        } else if (arg.rfind("--data-dir=", 0) == 0) {
            options.dataDirectory = arg.substr(11);
        } else if (arg == "--no-color") {
            options.color = 0;
        } else if (arg == "--color") {
            options.color = 1;
        } else if (arg == "--ascii") {
            options.unicode = 0;
        } else if (arg == "--unicode") {
            options.unicode = 1;
        } else if (arg == "--no-anim") {
            options.animations = false;
        } else if (arg == "-h" || arg == "--help") {
            options.showHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            options.showVersion = true;
        } else {
            error = "Unknown option: " + arg;
            return false;
        }
    }
    if (util::trim(options.dataDirectory).empty()) {
        error = "The data directory name cannot be empty.";
        return false;
    }
    return true;
}

std::string AppOptions::usage()
{
    return "RESQ - Rescue and Emergency Support System (academic prototype)\n"
           "\n"
           "Usage: resq [options]\n"
           "\n"
           "  --data-dir DIR   Folder for emergencies.txt, resources.txt and\n"
           "                   assignments.txt (default: ./data)\n"
           "  --no-color       Disable ANSI colors (also honours NO_COLOR)\n"
           "  --color          Force colors on\n"
           "  --ascii          Use plain ASCII borders instead of box drawing\n"
           "  --unicode        Force Unicode box drawing\n"
           "  --no-anim        Disable loading animations\n"
           "  -v, --version    Show the version\n"
           "  -h, --help       Show this help\n";
}

RESQApplication::RESQApplication(const AppOptions& options)
    : options_(options),
      console_(std::cin, std::cout, resq_term_init() == 1 || options.color == 1,
               options.color != 0, options.unicode == -1 ? resq_term_enable_utf8() == 1 : options.unicode == 1,
               options.animations, resq_stdin_is_tty() == 1, resq_stdout_is_tty() == 1),
      files_(options.dataDirectory),
      dispatcher_(emergencies_, resources_),
      analytics_(emergencies_, resources_)
{
    if (options.unicode == 1) {
        resq_term_enable_utf8();
    }
    console_.pushScreen("Home");
}

std::time_t RESQApplication::now() const { return std::time(nullptr); }

int RESQApplication::run()
{
    try {
        startup();
        mainLoop();
    } catch (const InputClosed&) {
        console_.info("Input closed - saving and exiting.");
    }
    shutdown();
    return exitCode_;
}

void RESQApplication::startup()
{
    console_.clearScreen();
    console_.printLogo();
    console_.out() << '\n';
    console_.spinner("Loading records from " + files_.dataDirectory() + " ...", 700);

    loadReport_ = files_.loadAll(emergencies_, resources_);
    reconcileNotes_ = dispatcher_.reconcile(now());

    if (loadReport_.anyFileFound()) {
        console_.success("Loaded " + std::to_string(loadReport_.emergenciesLoaded) + " emergencies, " +
                         std::to_string(loadReport_.resourcesLoaded) + " resources and " +
                         std::to_string(loadReport_.assignmentsLoaded) + " assignment records from " +
                         files_.dataDirectory() + ".");
    } else {
        console_.info("No saved data found in \"" + files_.dataDirectory() +
                      "\" - starting with an empty system. Files are created on the first save.");
    }
    const size_t shown = std::min<size_t>(loadReport_.warnings.size(), 6);
    for (size_t i = 0; i < shown; ++i) {
        console_.warning(loadReport_.warnings[i]);
    }
    if (loadReport_.warnings.size() > shown) {
        console_.info(std::to_string(loadReport_.warnings.size() - shown) +
                      " more file warning(s): see Settings > Data file status.");
    }
    for (const std::string& path : loadReport_.preserved) {
        console_.info("The original file was kept unchanged as " + path);
    }
    for (const std::string& note : reconcileNotes_) {
        console_.warning("Data check: " + note);
    }
    if (!reconcileNotes_.empty()) {
        persist();
    }
    const int columns = resq_term_columns();
    if (columns > 0 && columns < Console::kWidth) {
        console_.warning("This terminal is " + std::to_string(columns) +
                         " columns wide. RESQ is laid out for 80 columns; widen the window or use landscape mode.");
    }
    if (!loadReport_.anyFileFound() && emergencies_.empty() && resources_.empty() && console_.interactiveInput()) {
        console_.out() << '\n';
        if (console_.confirm("Load FICTIONAL demonstration data to explore RESQ?")) {
            const DemoResult result = DemoData::load(emergencies_, resources_, dispatcher_, now());
            persist();
            log("Demo data loaded");
            console_.success("Added " + std::to_string(result.emergenciesAdded) + " fictional cases and " +
                             std::to_string(result.resourcesAdded) + " fictional resources.");
        }
    }
    console_.pause();
}

void RESQApplication::mainLoop()
{
    while (true) {
        showDashboard();
        const std::string choice = console_.readChoice("Select an option");
        if (choice == "0") {
            return;
        }
        try {
            if (choice == "1") {
                registerEmergency();
            } else if (choice == "2") {
                viewEmergencies();
            } else if (choice == "3") {
                searchEmergencies();
            } else if (choice == "4") {
                updateEmergency();
            } else if (choice == "5") {
                dispatchMenu();
            } else if (choice == "6") {
                resourceMenu();
            } else if (choice == "7") {
                reportsMenu();
            } else if (choice == "8") {
                demoMenu();
            } else if (choice == "9") {
                settingsMenu();
            } else if (choice == "H" || choice == "?") {
                showHelp();
            } else {
                console_.error("\"" + choice + "\" is not a menu option. Enter a number from 0 to 9, or H for help.");
                console_.pause();
            }
        } catch (const InputCancelled&) {
            console_.warning("Cancelled - no further changes were made.");
            console_.pause();
        } catch (const InputClosed&) {
            throw;
        } catch (const std::exception& ex) {
            console_.error(std::string("Unexpected error: ") + ex.what() + ". The action was stopped; data is safe.");
            console_.pause();
        }
    }
}

void RESQApplication::shutdown()
{
    console_.out() << '\n';
    console_.spinner("Saving records ...", 500);
    if (persist()) {
        console_.success("All records saved to " + files_.dataDirectory() + ".");
    } else {
        exitCode_ = 1;
    }
    console_.out() << '\n';
    console_.printBox("RESQ SESSION CLOSED",
                      {console_.paint("Thank you for using RESQ.", Style::Highlight),
                       "Emergencies: " + std::to_string(emergencies_.size()) +
                           "   Resources: " + std::to_string(resources_.size()),
                       console_.paint("Stay safe. In a real emergency, contact your local emergency number.",
                                      Style::Muted)},
                      Style::Danger);
}

bool RESQApplication::persist()
{
    std::string error;
    if (files_.saveAll(emergencies_, resources_, error)) {
        lastSaved_ = now();
        return true;
    }
    console_.error("Could not save data: " + error);
    console_.warning("Changes are kept in memory; RESQ will retry on the next change and at exit.");
    return false;
}

void RESQApplication::log(const std::string& activity)
{
    recentActivity_.push_front(util::formatClock(now()) + "  " + activity);
    while (recentActivity_.size() > 4) {
        recentActivity_.pop_back();
    }
}

std::vector<std::string> RESQApplication::statusLines(const std::vector<Row>& rows, int width) const
{
    std::vector<std::string> lines;
    for (const Row& row : rows) {
        const std::string& value = row.second.first;
        const int labelWidth = width - Console::visibleWidth(value) - 1;
        std::string label = row.first + " ";
        while (Console::visibleWidth(label) < labelWidth) {
            label += '.';
        }
        lines.push_back(console_.paint(label, Style::Plain) + " " + console_.paint(value, row.second.second));
    }
    return lines;
}

void RESQApplication::showDashboard()
{
    console_.clearScreen();
    console_.printLogo();
    const std::time_t t = now();
    emergencies_.recalculateAll(t);
    const Statistics s = analytics_.compute();
    const double utilization = s.utilizationPercent();

    const int boxWidth = 38;
    const std::vector<std::string> left = console_.renderBox(
        "EMERGENCY CASES",
        statusLines({{"Total cases", {std::to_string(s.totalEmergencies), Style::Highlight}},
                     {"Pending", {std::to_string(s.count(Status::Pending)), Style::Warning}},
                     {"Assigned", {std::to_string(s.count(Status::Assigned)), Style::Accent}},
                     {"In progress", {std::to_string(s.count(Status::InProgress)), Style::Progress}},
                     {"Resolved", {std::to_string(s.count(Status::Resolved)), Style::Success}},
                     {"Cancelled", {std::to_string(s.count(Status::Cancelled)), Style::Muted}},
                     {"Critical (still active)", {std::to_string(s.activeCritical), Style::Critical}}},
                    boxWidth - 4),
        boxWidth, Style::Accent);
    const std::vector<std::string> right = console_.renderBox(
        "RESCUE RESOURCES",
        statusLines({{"Total resources", {std::to_string(s.totalResources), Style::Highlight}},
                     {"Available", {std::to_string(s.availableResources), Style::Success}},
                     {"Deployed", {std::to_string(s.deployedResources), Style::Warning}},
                     {"Out of service", {std::to_string(s.outOfServiceResources), Style::Danger}},
                     {"Active assignments", {std::to_string(s.activeAssignments), Style::Accent}},
                     {"Utilization (in service)",
                      {utilization < 0 ? "-" : std::to_string(static_cast<int>(std::lround(utilization))) + "%",
                       Style::Highlight}},
                     {"Assignments logged", {std::to_string(s.totalAssignmentsLogged), Style::Muted}}},
                    boxWidth - 4),
        boxWidth, Style::Accent);
    console_.printSideBySide(left, right);

    const Emergency* next = emergencies_.highestPriorityPending(t);
    console_.out() << '\n';
    if (next != nullptr) {
        console_.out() << "  " << console_.paint(" NEXT IN QUEUE ", Style::Badge) << "  "
                       << console_.paint(next->id(), Style::Highlight) << "  "
                       << console_.paint(util::toUpper(toString(next->severity())), severityStyle(next->severity()))
                       << "  " << toString(next->category()) << "  score "
                       << console_.paint(std::to_string(next->priorityScore()), Style::Highlight) << '\n'
                       << std::string(19, ' ')
                       << console_.fit(next->location() + "  |  waiting " +
                                           util::formatDuration(t - next->registeredAt()) +
                                           "  |  dispatch: menu 5 > 1",
                                       Console::kWidth - 19)
                       << '\n';
    } else {
        console_.out() << "  " << console_.paint("Queue clear: no pending emergencies.", Style::Success) << '\n';
    }
    if (!recentActivity_.empty()) {
        console_.out() << "  " << console_.paint("Recent:", Style::Muted);
        for (size_t i = 0; i < recentActivity_.size() && i < 2; ++i) {
            console_.out() << (i == 0 ? " " : "\n          ") << console_.paint(recentActivity_[i], Style::Muted);
        }
        console_.out() << '\n';
    }

    console_.printSection("Main menu");
    console_.printMenu({{"1", "Register emergency"},
                        {"2", "View all emergencies"},
                        {"3", "Search emergencies"},
                        {"4", "Update emergency"},
                        {"5", "Dispatch & operations"},
                        {"6", "Resource management"},
                        {"7", "Reports & analytics"},
                        {"8", "Demonstration data"},
                        {"9", "Settings & data files"},
                        {"H", "Help & about"},
                        {"0", "Save & exit"}});
    console_.out() << "  "
                   << console_.paint("Data folder: " + files_.dataDirectory() + "  |  changes are saved automatically" +
                                         (lastSaved_ > 0 ? "  |  last save " + util::formatClock(lastSaved_) : ""),
                                     Style::Muted)
                   << "\n\n";
}

void RESQApplication::printEmergencyCard(const Emergency& e)
{
    const std::time_t t = now();
    const auto kv = [this](const std::string& label, const std::string& value) {
        return console_.paint(console_.fit(label, 16), Style::Muted) + ": " + value;
    };
    std::string timing;
    switch (e.status()) {
    case Status::Pending: timing = "waiting " + util::formatDuration(t - e.registeredAt()); break;
    case Status::Assigned:
    case Status::InProgress:
        timing = "first response after " + util::formatDuration(e.assignedAt() - e.registeredAt());
        break;
    case Status::Resolved: timing = "resolved after " + util::formatDuration(e.closedAt() - e.registeredAt()); break;
    case Status::Cancelled: timing = "cancelled"; break;
    }

    std::vector<std::string> lines{
        kv("Category", toString(e.category())),
        kv("Severity", console_.paint(util::toUpper(toString(e.severity())), severityStyle(e.severity()))) +
            "      " + console_.paint("Priority score: ", Style::Muted) +
            console_.paint(std::to_string(e.priorityScore()), Style::Highlight),
        kv("Status", console_.paint(toString(e.status()), statusStyle(e.status()))),
        kv("Location", e.location()),
        kv("Reported by", e.reporter()),
        kv("Contact", e.contact()),
        kv("People affected", std::to_string(e.affected())),
        kv("Registered", util::formatDateTime(e.registeredAt()) + "  (" + timing + ")"),
    };
    if (e.assignedAt() > 0) {
        lines.push_back(kv("Assigned", util::formatDateTime(e.assignedAt())));
    }
    if (e.startedAt() > 0) {
        lines.push_back(kv("Work started", util::formatDateTime(e.startedAt())));
    }
    if (e.closedAt() > 0) {
        lines.push_back(kv(e.status() == Status::Resolved ? "Resolved" : "Closed", util::formatDateTime(e.closedAt())));
    }
    const std::vector<std::string> description = util::wrapText(e.description(), 54);
    for (size_t i = 0; i < description.size(); ++i) {
        lines.push_back(i == 0 ? kv("Description", description[i]) : std::string(18, ' ') + description[i]);
    }
    lines.push_back(kv("Resources", resourceList(resources_.assignedTo(e.id()))));

    const std::string title = "EMERGENCY " + e.id() + (e.isDemo() ? "  [FICTIONAL DEMO RECORD]" : "");
    console_.printBox(title, lines, e.severity() == Severity::Critical && e.isActive() ? Style::Danger : Style::Accent);
}

void RESQApplication::printScoreBreakdown(const Emergency& e)
{
    const PriorityBreakdown b = e.priorityBreakdown(now());
    console_.out() << "  " << console_.paint("Priority " + std::to_string(b.total()), Style::Highlight) << " = severity "
                   << b.severityPoints << " + people " << b.affectedPoints << " + waiting " << b.waitingPoints << " ("
                   << b.waitingMinutes << " min) + category " << b.categoryPoints << '\n'
                   << "  "
                   << console_.paint("The score only orders the queue; it is not a risk prediction. "
                                     "The operator decides.",
                                     Style::Muted)
                   << '\n';
}

void RESQApplication::printEmergencyTable(const std::vector<const Emergency*>& list)
{
    Table table({{"ID", 9},
                 {"Category", 12},
                 {"Location", 15},
                 {"Severity", 8},
                 {"Score", 5, true},
                 {"Status", 11},
                 {"Registered", 11}});
    bool demo = false;
    for (const Emergency* e : list) {
        demo = demo || e->isDemo();
        table.addRow({{e->id() + (e->isDemo() ? "*" : ""), Style::Bold},
                      {shortName(e->category())},
                      {e->location()},
                      {toString(e->severity()), severityStyle(e->severity())},
                      {std::to_string(e->priorityScore()), Style::Highlight},
                      {toString(e->status()), statusStyle(e->status())},
                      {util::formatCompact(e->registeredAt()), Style::Muted}});
    }
    table.print(console_);
    if (demo) {
        console_.out() << "  " << console_.paint("* fictional demonstration record", Style::Muted) << '\n';
    }
}

void RESQApplication::printResourceTable(const std::vector<const Resource*>& list)
{
    Table table({{"ID", 9}, {"Type", 14}, {"Location", 15}, {"Status", 15}, {"Cap", 4, true}, {"Assigned to", 15}});
    bool demo = false;
    for (const Resource* r : list) {
        demo = demo || r->isDemo();
        std::string assigned;
        for (const std::string& eid : r->assignedEmergencies()) {
            assigned += (assigned.empty() ? "" : ",") + eid;
        }
        table.addRow({{r->id() + (r->isDemo() ? "*" : ""), Style::Bold},
                      {shortName(r->type())},
                      {r->location()},
                      {r->statusLabel(), resourceStyle(r->status())},
                      {std::to_string(r->capacity())},
                      {assigned.empty() ? "-" : assigned, assigned.empty() ? Style::Muted : Style::Plain}});
    }
    table.print(console_);
    if (demo) {
        console_.out() << "  " << console_.paint("* fictional demonstration record", Style::Muted) << '\n';
    }
}

void RESQApplication::printResourceCard(const Resource& r)
{
    const auto kv = [this](const std::string& label, const std::string& value) {
        return console_.paint(console_.fit(label, 18), Style::Muted) + ": " + value;
    };
    int deployments = 0;
    for (const AssignmentRecord& record : resources_.history()) {
        deployments += record.resourceId == r.id() ? 1 : 0;
    }
    std::string assigned;
    for (const std::string& eid : r.assignedEmergencies()) {
        const Emergency* e = emergencies_.find(eid);
        assigned += (assigned.empty() ? "" : ", ") + eid + (e ? " (" + toString(e->status()) + ")" : "");
    }
    console_.printBox("RESOURCE " + r.id() + (r.isDemo() ? "  [FICTIONAL DEMO RECORD]" : ""),
                      {kv("Type", toString(r.type())),
                       kv("Current location", r.location()),
                       kv("Availability", console_.paint(r.statusLabel(), resourceStyle(r.status()))),
                       kv("Capacity", r.capacity() == 0 ? "not applicable" : std::to_string(r.capacity()) + " people"),
                       kv("Max simultaneous", std::to_string(r.maxConcurrent()) +
                                                  (r.maxConcurrent() == 1 ? " (exclusive use)" : " (shared use)")),
                       kv("Assigned to", assigned.empty() ? "-" : assigned),
                       kv("Deployments logged", std::to_string(deployments))});
}

std::vector<SuitableResource> RESQApplication::printSuitableResources(const Emergency& e)
{
    const std::vector<SuitableResource> list = resources_.suitableFor(e);
    const int capacity = resources_.capacityAssignedTo(e.id());
    console_.out() << "  People affected: " << e.affected() << "   Capacity already assigned: " << capacity << '\n';
    if (e.affected() > 0 && capacity < e.affected() && !resources_.assignedTo(e.id()).empty()) {
        console_.warning("Assigned capacity is below the number of people affected; consider more resources.");
    }
    if (list.empty()) {
        console_.warning("No suitable resource is available right now. Options: release one from a lower-priority "
                         "case, return one to service, or add a new resource (A lists all available).");
        return list;
    }
    Table table({{"#", 2, true}, {"ID", 9}, {"Type", 14}, {"Location", 16}, {"Cap", 4, true}, {"Load", 5}, {"Match", 21}});
    for (size_t i = 0; i < list.size(); ++i) {
        const Resource& r = *list[i].resource;
        table.addRow({{std::to_string(i + 1), Style::Heading},
                      {r.id(), Style::Bold},
                      {shortName(r.type())},
                      {r.location()},
                      {std::to_string(r.capacity())},
                      {std::to_string(r.activeAssignments()) + "/" + std::to_string(r.maxConcurrent())},
                      {list[i].reason, list[i].typeRank == 0 ? Style::Success : Style::Plain}});
    }
    table.print(console_);
    console_.out() << "  "
                   << console_.paint("Matching uses resource type and location names only (no maps or GPS).",
                                     Style::Muted)
                   << '\n';
    return list;
}

Category RESQApplication::pickCategory()
{
    std::vector<std::pair<std::string, std::string>> items;
    for (Category c : allCategories()) {
        items.emplace_back(std::to_string(static_cast<int>(c)), toString(c));
    }
    console_.out() << "  " << console_.paint("Emergency category:", Style::Accent) << '\n';
    console_.printMenu(items);
    Category result = Category::Other;
    fromNumber(static_cast<int>(console_.readNumber("Category (1-7)", 1, kCategoryCount)), result);
    return result;
}

Severity RESQApplication::pickSeverity()
{
    const std::vector<std::pair<Severity, std::string>> options{
        {Severity::Critical, "immediate threat to life or a spreading hazard"},
        {Severity::High, "serious and urgent, not immediately life-threatening"},
        {Severity::Medium, "needs a response; situation currently stable"},
        {Severity::Low, "minor; no immediate danger"},
    };
    console_.out() << "  " << console_.paint("Severity level:", Style::Accent) << '\n';
    for (const auto& option : options) {
        console_.out() << "  " << console_.paint("[" + std::to_string(static_cast<int>(option.first)) + "]", Style::Heading)
                       << ' ' << console_.paint(console_.fit(toString(option.first), 9), severityStyle(option.first))
                       << "- " << option.second << '\n';
    }
    Severity result = Severity::Low;
    fromNumber(static_cast<int>(console_.readNumber("Severity (1-4)", 1, kSeverityCount)), result);
    return result;
}

ResourceType RESQApplication::pickResourceType()
{
    std::vector<std::pair<std::string, std::string>> items;
    for (ResourceType type : allResourceTypes()) {
        items.emplace_back(std::to_string(static_cast<int>(type)), toString(type));
    }
    console_.out() << "  " << console_.paint("Resource type:", Style::Accent) << '\n';
    console_.printMenu(items);
    ResourceType result = ResourceType::Ambulance;
    fromNumber(static_cast<int>(console_.readNumber("Type (1-6)", 1, kResourceTypeCount)), result);
    return result;
}

std::string RESQApplication::readValidated(const std::string& prompt, size_t maxLength,
                                           const std::function<std::string(const std::string&)>& validate)
{
    while (true) {
        const std::string value = console_.readText(prompt, maxLength);
        const std::string error = validate(value);
        if (error.empty()) {
            return value;
        }
        console_.error(error);
    }
}

std::time_t RESQApplication::readRegistrationTime()
{
    const std::time_t current = now();
    console_.out() << "  " << console_.paint("Registration time: press Enter for now (" + util::formatDateTime(current) +
                                                 ") or type YYYY-MM-DD HH:MM",
                                             Style::Muted)
                   << '\n';
    while (true) {
        const std::string value = console_.readText("Registration time", 16, false);
        if (value.empty()) {
            return current;
        }
        std::time_t parsed = 0;
        if (!util::parseDateTime(value, parsed)) {
            console_.error("Use the format YYYY-MM-DD HH:MM, for example 2026-10-18 14:30.");
            continue;
        }
        if (parsed > current + 300) {
            console_.error("The registration time cannot be in the future.");
            continue;
        }
        if (current - parsed > 7 * 24 * 3600 &&
            !console_.confirm("That is more than 7 days ago. Use it anyway?")) {
            continue;
        }
        return parsed;
    }
}

Emergency* RESQApplication::promptEmergency(const std::string& prompt)
{
    const std::string id = console_.readText(prompt, 16);
    Emergency* e = emergencies_.find(id);
    if (e == nullptr) {
        console_.error("No emergency with ID " + util::toUpper(id) + " exists. Use Search (menu 3) to look it up.");
    }
    return e;
}

Resource* RESQApplication::promptResource(const std::string& prompt)
{
    const std::string id = console_.readText(prompt, 16);
    Resource* r = resources_.find(id);
    if (r == nullptr) {
        console_.error("No resource with ID " + util::toUpper(id) + " exists. Use View or Search to find it.");
    }
    return r;
}

void RESQApplication::registerEmergency()
{
    ScreenScope scope(console_, "Register emergency");
    console_.printHeader("Register a new emergency");
    console_.info("Type :q at any prompt to cancel. All fields are required unless a default is shown.");
    console_.out() << '\n';

    const std::string suggested = emergencies_.generateId();
    std::string id;
    while (true) {
        const std::string input = console_.readText("Emergency ID [Enter = " + suggested + "]", 8, false);
        id = input.empty() ? suggested : util::toUpper(input);
        std::string error = Emergency::validateId(id);
        if (error.empty() && emergencies_.exists(id)) {
            error = "An emergency with ID " + id + " already exists. IDs must be unique.";
        }
        if (error.empty()) {
            break;
        }
        console_.error(error);
    }
    const std::string reporter = readValidated("Reporter's name", Emergency::kMaxReporter, Emergency::validateReporter);
    const Category category = pickCategory();
    const std::string location = readValidated("Location", Emergency::kMaxLocation, Emergency::validateLocation);
    const std::string contact =
        readValidated("Contact phone or e-mail", Emergency::kMaxContact, Emergency::validateContact);
    const std::string description =
        readValidated("Description (max 200 chars)", Emergency::kMaxDescription, Emergency::validateDescription);
    const Severity severity = pickSeverity();
    const int affected = static_cast<int>(console_.readNumber("People affected (0 if none)", 0, Emergency::kMaxAffected));
    const std::time_t registered = readRegistrationTime();

    Emergency emergency(id, reporter, category, location, contact, description, severity, affected, registered);
    emergency.recalculatePriority(now());
    console_.out() << '\n';
    printEmergencyCard(emergency);
    if (!console_.confirm("Register this emergency?", true)) {
        console_.warning("Not registered. Nothing was saved.");
        console_.pause();
        return;
    }
    const std::string error = emergencies_.add(emergency);
    if (!error.empty()) {
        console_.error(error);
        console_.pause();
        return;
    }
    persist();
    log("Registered " + id + " (" + shortName(category) + ", " + toString(severity) + ")");
    const int position = emergencies_.queuePosition(id, now());
    const int queueSize = static_cast<int>(emergencies_.pendingByPriority(now()).size());
    console_.success("Emergency " + id + " registered with priority score " +
                     std::to_string(emergencies_.find(id)->priorityScore()) + " - position " +
                     std::to_string(position) + " of " + std::to_string(queueSize) + " in the pending queue.");
    printScoreBreakdown(*emergencies_.find(id));
    if (severity == Severity::Critical) {
        console_.warning("CRITICAL case: dispatch resources as soon as possible.");
    }
    if (console_.confirm("Assign a resource to " + id + " now?")) {
        if (assignResourceFlow(id) && console_.confirm("Have crews started work on scene? Mark In Progress now?")) {
            const OpResult result = dispatcher_.startOperation(id, now());
            result.ok ? console_.success(result.message) : console_.error(result.message);
            if (result.ok) {
                persist();
                log(id + " in progress");
            }
        }
    }
    console_.pause();
}

void RESQApplication::viewEmergencies()
{
    ScreenScope scope(console_, "All emergencies");
    EmergencySortKey key = EmergencySortKey::Priority;
    bool reverse = false;
    while (true) {
        console_.printHeader("All emergencies");
        if (emergencies_.empty()) {
            console_.info("No emergencies registered yet. Register one (menu 1) or load demo data (menu 8).");
            console_.pause();
            return;
        }
        emergencies_.recalculateAll(now());
        std::string order;
        switch (key) {
        case EmergencySortKey::Severity: order = "severity (Critical first)"; break;
        case EmergencySortKey::Priority: order = "priority score (highest first)"; break;
        case EmergencySortKey::RegistrationTime: order = "registration time (oldest first)"; break;
        case EmergencySortKey::Id: order = "emergency ID (A-Z)"; break;
        }
        console_.out() << "  " << emergencies_.size() << " record(s), sorted by " << order
                       << (reverse ? " - reversed" : "") << "\n\n";
        printEmergencyTable(emergencies_.sorted(key, reverse));
        console_.out() << '\n';
        console_.printMenu({{"1", "Sort by severity"},
                            {"2", "Sort by priority score"},
                            {"3", "Sort by registration time"},
                            {"4", "Sort by emergency ID"},
                            {"R", "Reverse the order"},
                            {"D", "Show full details of a case"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            key = EmergencySortKey::Severity;
            reverse = false;
        } else if (choice == "2") {
            key = EmergencySortKey::Priority;
            reverse = false;
        } else if (choice == "3") {
            key = EmergencySortKey::RegistrationTime;
            reverse = false;
        } else if (choice == "4") {
            key = EmergencySortKey::Id;
            reverse = false;
        } else if (choice == "R") {
            reverse = !reverse;
        } else if (choice == "D") {
            if (const Emergency* e = promptEmergency("Emergency ID")) {
                printEmergencyCard(*e);
                printScoreBreakdown(*e);
            }
            console_.pause();
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

void RESQApplication::searchEmergencies()
{
    ScreenScope scope(console_, "Search");
    while (true) {
        console_.printHeader("Search emergencies");
        if (emergencies_.empty()) {
            console_.info("There are no emergencies to search yet.");
            console_.pause();
            return;
        }
        console_.printMenu({{"1", "By emergency ID"},
                            {"2", "By reporter's name"},
                            {"3", "By location"},
                            {"4", "By category"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Search by");
        std::vector<const Emergency*> results;
        std::string description;
        if (choice == "0") {
            return;
        } else if (choice == "1" || choice == "2" || choice == "3") {
            const EmergencySearchField field = choice == "1"   ? EmergencySearchField::Id
                                               : choice == "2" ? EmergencySearchField::Reporter
                                                               : EmergencySearchField::Location;
            const std::string query = console_.readText("Search text (part of a word is enough)", 40);
            results = emergencies_.search(field, query);
            description = "\"" + query + "\"";
        } else if (choice == "4") {
            const Category category = pickCategory();
            results = emergencies_.searchByCategory(category);
            description = "category " + toString(category);
        } else {
            console_.error("Invalid option.");
            console_.pause();
            continue;
        }

        emergencies_.recalculateAll(now());
        console_.out() << '\n';
        if (results.empty()) {
            console_.warning("No emergencies match " + description + ".");
        } else {
            console_.success(std::to_string(results.size()) + " match(es) for " + description + ".");
            console_.out() << '\n';
            if (results.size() <= 5) {
                for (const Emergency* e : results) {
                    printEmergencyCard(*e);
                }
            } else {
                printEmergencyTable(results);
                console_.out() << '\n';
                const std::string id = console_.readText("ID for full details (Enter to skip)", 16, false);
                if (!id.empty()) {
                    if (const Emergency* e = emergencies_.find(id)) {
                        printEmergencyCard(*e);
                    } else {
                        console_.error("No emergency with ID " + util::toUpper(id) + ".");
                    }
                }
            }
        }
        console_.pause();
    }
}

void RESQApplication::updateEmergency()
{
    ScreenScope scope(console_, "Update emergency");
    console_.printHeader("Update emergency");
    if (emergencies_.empty()) {
        console_.info("There are no emergencies to update yet.");
        console_.pause();
        return;
    }
    Emergency* found = promptEmergency("Emergency ID to update");
    if (found == nullptr) {
        console_.pause();
        return;
    }
    const std::string id = found->id();
    while (true) {
        const Emergency* e = emergencies_.find(id);
        console_.printHeader("Update emergency " + id);
        emergencies_.recalculateAll(now());
        printEmergencyCard(*e);
        console_.out() << '\n';
        console_.printMenu({{"1", "Change severity"},
                            {"2", "Update location"},
                            {"3", "Edit description"},
                            {"4", "Update people affected"},
                            {"5", "Change case status"},
                            {"0", "Done"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        }
        const int before = e->priorityScore();
        std::string error;
        std::string change;
        if ((choice == "1" || choice == "2" || choice == "4") && e->isClosed()) {
            console_.error("Case " + id + " is " + toString(e->status()) +
                           "; only its description can still be corrected.");
            console_.pause();
            continue;
        }
        if (choice == "1") {
            const Severity old = e->severity();
            const Severity severity = pickSeverity();
            error = emergencies_.updateSeverity(id, severity, now());
            change = "Severity " + toString(old) + " -> " + toString(severity);
        } else if (choice == "2") {
            const std::string location = readValidated("New location", Emergency::kMaxLocation, Emergency::validateLocation);
            error = emergencies_.updateLocation(id, location, now());
            change = "Location updated to " + location;
        } else if (choice == "3") {
            const std::string text =
                readValidated("New description", Emergency::kMaxDescription, Emergency::validateDescription);
            error = emergencies_.updateDescription(id, text, now());
            change = "Description updated";
        } else if (choice == "4") {
            const int old = e->affected();
            const int affected =
                static_cast<int>(console_.readNumber("People affected", 0, Emergency::kMaxAffected));
            error = emergencies_.updateAffected(id, affected, now());
            change = "People affected " + std::to_string(old) + " -> " + std::to_string(affected);
        } else if (choice == "5") {
            changeStatusFlow(id);
            console_.pause();
            continue;
        } else {
            console_.error("Invalid option.");
            console_.pause();
            continue;
        }
        if (!error.empty()) {
            console_.error(error);
        } else {
            persist();
            log(id + ": " + change);
            const int after = emergencies_.find(id)->priorityScore();
            console_.success(change + ". Priority score " + std::to_string(before) + " -> " + std::to_string(after) +
                             ".");
        }
        console_.pause();
    }
}

void RESQApplication::changeStatusFlow(const std::string& emergencyId)
{
    const Emergency* e = emergencies_.find(emergencyId);
    const std::vector<Status> allowed = dispatcher_.allowedNextStatuses(emergencyId);
    if (e == nullptr) {
        return;
    }
    if (allowed.empty()) {
        console_.info("Case " + e->id() + " is " + toString(e->status()) + "; closed cases cannot change status.");
        return;
    }
    console_.out() << "  Current status: " << console_.paint(toString(e->status()), statusStyle(e->status()))
                   << ". Allowed next steps:\n";
    for (size_t i = 0; i < allowed.size(); ++i) {
        console_.out() << "  " << console_.paint("[" + std::to_string(i + 1) + "]", Style::Heading) << ' '
                       << console_.paint(console_.fit(toString(allowed[i]), 12), statusStyle(allowed[i]))
                       << statusHint(allowed[i]) << '\n';
    }
    console_.out() << "  " << console_.paint("[0]", Style::Heading) << " Keep the current status\n";
    const long long choice = console_.readNumber("Choose", 0, static_cast<long long>(allowed.size()));
    if (choice == 0) {
        return;
    }
    const Status target = allowed[static_cast<size_t>(choice - 1)];
    if (target == Status::Assigned) {
        assignResourceFlow(emergencyId);
        return;
    }
    if ((target == Status::Resolved || target == Status::Cancelled || target == Status::Pending) &&
        !console_.confirm("Change " + e->id() + " to " + toString(target) + "? Assigned resources will be released.")) {
        console_.warning("Status unchanged.");
        return;
    }
    const OpResult result = dispatcher_.changeStatus(emergencyId, target, now());
    if (result.ok) {
        persist();
        log(util::toUpper(emergencyId) + " -> " + toString(target));
        console_.success(result.message);
    } else {
        console_.error(result.message);
    }
}

void RESQApplication::dispatchMenu()
{
    ScreenScope scope(console_, "Dispatch");
    while (true) {
        console_.printHeader("Dispatch & operations");
        const size_t pending = emergencies_.withStatus(Status::Pending).size();
        const size_t active =
            emergencies_.withStatus(Status::Assigned).size() + emergencies_.withStatus(Status::InProgress).size();
        console_.out() << "  Pending in queue: " << console_.paint(std::to_string(pending), Style::Warning)
                       << "    Active operations: " << console_.paint(std::to_string(active), Style::Accent)
                       << "    Resources ready: "
                       << console_.paint(std::to_string(resources_.acceptingAssignments().size()), Style::Success)
                       << "\n\n";
        console_.printMenu({{"1", "Process next highest-priority case"},
                            {"2", "View the pending priority queue"},
                            {"3", "Manage active operations"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            processNextEmergency();
        } else if (choice == "2") {
            viewPriorityQueue();
        } else if (choice == "3") {
            manageActiveOperations();
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

void RESQApplication::viewPriorityQueue()
{
    ScreenScope scope(console_, "Priority queue");
    console_.printHeader("Pending priority queue");
    const std::vector<const Emergency*> queue = emergencies_.pendingByPriority(now());
    if (queue.empty()) {
        console_.success("No pending emergencies - the queue is clear.");
    } else {
        console_.out() << "  " << queue.size() << " pending case(s), highest priority first:\n\n";
        printEmergencyTable(queue);
        console_.out() << '\n';
        console_.out() << "  " << console_.paint("Ordered by a std::priority_queue on score, then registration time.",
                                                 Style::Muted)
                       << '\n';
    }
    console_.pause();
}

void RESQApplication::processNextEmergency()
{
    ScreenScope scope(console_, "Process next");
    std::string selected;
    while (true) {
        const std::vector<const Emergency*> queue = emergencies_.pendingByPriority(now());
        console_.printHeader("Process next emergency");
        if (queue.empty()) {
            console_.success("The pending queue is empty - every case is assigned, in progress or closed.");
            console_.pause();
            return;
        }
        const Emergency* e = selected.empty() ? nullptr : emergencies_.find(selected);
        if (e == nullptr || e->status() != Status::Pending) {
            e = queue.front();
            selected.clear();
        }
        const std::string id = e->id();
        const int position = emergencies_.queuePosition(id, now());
        console_.out() << "  Queue position " << console_.paint(std::to_string(position), Style::Highlight) << " of "
                       << queue.size()
                       << (position == 1 ? " - highest priority pending case" : " - selected by the operator")
                       << "\n";
        printEmergencyCard(*e);
        printScoreBreakdown(*e);
        console_.printSection("Suitable available resources");
        printSuitableResources(*e);
        console_.out() << '\n';
        console_.printMenu({{"1", "Assign resource(s) to this case"},
                            {"2", "Choose a different pending case"},
                            {"3", "Cancel this case (false alarm)"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            if (assignResourceFlow(id) &&
                console_.confirm("Have crews started work on scene? Mark " + id + " In Progress now?")) {
                const OpResult result = dispatcher_.startOperation(id, now());
                result.ok ? console_.success(result.message) : console_.error(result.message);
                if (result.ok) {
                    persist();
                    log(id + " in progress");
                }
            }
            selected.clear();
            console_.pause();
        } else if (choice == "2") {
            printEmergencyTable(queue);
            const std::string other = console_.readText("Pending case ID", 16);
            const Emergency* candidate = emergencies_.find(other);
            if (candidate == nullptr || candidate->status() != Status::Pending) {
                console_.error(util::toUpper(other) + " is not a pending case.");
                console_.pause();
            } else {
                selected = candidate->id();
            }
        } else if (choice == "3") {
            if (console_.confirm("Cancel " + id + " as a false alarm or duplicate?")) {
                const OpResult result = dispatcher_.cancel(id, now());
                result.ok ? console_.success(result.message) : console_.error(result.message);
                if (result.ok) {
                    persist();
                    log(id + " cancelled");
                }
                console_.pause();
            }
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

bool RESQApplication::assignResourceFlow(const std::string& emergencyId)
{
    bool assignedAny = false;
    while (true) {
        const Emergency* e = emergencies_.find(emergencyId);
        if (e == nullptr || !e->isActive()) {
            console_.error("Resources can only be assigned to an existing, active case.");
            return assignedAny;
        }
        console_.printSection("Assign resources to " + e->id());
        console_.out() << "  " << console_.paint(util::toUpper(toString(e->severity())), severityStyle(e->severity()))
                       << "  " << toString(e->category()) << "  |  " << e->location() << "  |  status "
                       << console_.paint(toString(e->status()), statusStyle(e->status())) << '\n'
                       << "  Already assigned: " << resourceList(resources_.assignedTo(e->id())) << '\n';
        const std::vector<SuitableResource> list = printSuitableResources(*e);
        const std::string input = util::toUpper(
            console_.readText("Resource ID or list # (A = all available, Enter = finish)", 16, false));
        if (input.empty()) {
            return assignedAny;
        }
        if (input == "A") {
            console_.out() << '\n';
            printResourceTable(resources_.acceptingAssignments());
            continue;
        }
        std::string resourceId = input;
        long long number = 0;
        if (util::parseInteger(input, number)) {
            if (number < 1 || number > static_cast<long long>(list.size())) {
                console_.error("There is no entry #" + input + " in the list.");
                continue;
            }
            resourceId = list[static_cast<size_t>(number - 1)].resource->id();
        }
        const Resource* r = resources_.find(resourceId);
        if (r == nullptr) {
            console_.error("No resource with ID " + resourceId + " exists.");
            continue;
        }
        if (r->canAccept() && !ResourceManager::isRecommended(r->type(), e->category()) &&
            !console_.confirm(r->id() + " is a " + toString(r->type()) + ", not a recommended type for " +
                              toString(e->category()) + ". Assign anyway?")) {
            continue;
        }
        const OpResult result = dispatcher_.assign(emergencyId, resourceId, now());
        if (!result.ok) {
            console_.error(result.message);
            continue;
        }
        persist();
        log("Assigned " + r->id() + " to " + e->id());
        console_.success(result.message);
        assignedAny = true;
        if (!console_.confirm("Assign another resource to " + e->id() + "?")) {
            return true;
        }
    }
}

void RESQApplication::manageActiveOperations()
{
    ScreenScope scope(console_, "Active operations");
    while (true) {
        console_.printHeader("Active operations");
        std::vector<const Emergency*> active;
        for (const Emergency* e : emergencies_.sorted(EmergencySortKey::Priority)) {
            if (e->status() == Status::Assigned || e->status() == Status::InProgress) {
                active.push_back(e);
            }
        }
        if (active.empty()) {
            console_.info("No active operations. Process a pending case first (Dispatch > 1).");
            console_.pause();
            return;
        }
        Table table({{"ID", 9}, {"Category", 12}, {"Location", 15}, {"Severity", 8}, {"Status", 11}, {"Res", 3, true},
                     {"Assigned", 12}});
        for (const Emergency* e : active) {
            table.addRow({{e->id() + (e->isDemo() ? "*" : ""), Style::Bold},
                          {shortName(e->category())},
                          {e->location()},
                          {toString(e->severity()), severityStyle(e->severity())},
                          {toString(e->status()), statusStyle(e->status())},
                          {std::to_string(resources_.assignedTo(e->id()).size())},
                          {util::formatCompact(e->assignedAt()), Style::Muted}});
        }
        table.print(console_);
        console_.out() << '\n';
        const std::string id = console_.readText("Case ID to manage (Enter = back)", 16, false);
        if (id.empty()) {
            return;
        }
        const Emergency* e = emergencies_.find(id);
        if (e == nullptr || !e->isActive()) {
            console_.error(util::toUpper(id) + " is not an active case.");
            console_.pause();
            continue;
        }
        manageOperation(e->id());
    }
}

void RESQApplication::manageOperation(const std::string& emergencyId)
{
    ScreenScope scope(console_, emergencyId);
    while (true) {
        const Emergency* e = emergencies_.find(emergencyId);
        if (e == nullptr) {
            return;
        }
        console_.printHeader("Operation " + emergencyId);
        printEmergencyCard(*e);
        if (e->isClosed()) {
            console_.info("This case is closed.");
            console_.pause();
            return;
        }
        console_.out() << '\n';
        console_.printMenu({{"1", "Assign an additional resource"},
                            {"2", "Release a resource"},
                            {"3", "Mark In Progress (work started)"},
                            {"4", "Mark Resolved (work complete)"},
                            {"5", "Return case to Pending queue"},
                            {"6", "Cancel case"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        OpResult result;
        bool attempted = true;
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            assignResourceFlow(emergencyId);
            attempted = false;
        } else if (choice == "2") {
            const auto assigned = resources_.assignedTo(emergencyId);
            if (assigned.empty()) {
                result = OpResult::failure("No resources are assigned to " + emergencyId + ".");
            } else {
                const std::string rid = assigned.size() == 1
                                            ? assigned.front()->id()
                                            : console_.readText("Resource ID to release (" + resourceList(assigned) + ")", 16);
                result = console_.confirm("Release " + util::toUpper(rid) + " from " + emergencyId + "?")
                             ? dispatcher_.release(emergencyId, rid, now())
                             : OpResult::failure("Release cancelled.");
            }
        } else if (choice == "3") {
            result = dispatcher_.startOperation(emergencyId, now());
        } else if (choice == "4") {
            result = console_.confirm("Confirm the operation for " + emergencyId +
                                      " is complete. All its resources will be released.")
                         ? dispatcher_.resolve(emergencyId, now())
                         : OpResult::failure("Case not resolved.");
        } else if (choice == "5") {
            result = console_.confirm("Return " + emergencyId + " to the queue and release its resources?")
                         ? dispatcher_.returnToQueue(emergencyId, now())
                         : OpResult::failure("Case unchanged.");
        } else if (choice == "6") {
            result = console_.confirm("Cancel " + emergencyId + "? All its resources will be released.")
                         ? dispatcher_.cancel(emergencyId, now())
                         : OpResult::failure("Case not cancelled.");
        } else {
            result = OpResult::failure("Invalid option.");
        }
        if (attempted) {
            if (result.ok) {
                persist();
                log(emergencyId + ": " + toString(emergencies_.find(emergencyId)->status()));
                console_.success(result.message);
            } else {
                console_.error(result.message);
            }
        }
        console_.pause();
    }
}

void RESQApplication::resourceMenu()
{
    ScreenScope scope(console_, "Resources");
    while (true) {
        console_.printHeader("Resource management");
        const Statistics s = analytics_.compute();
        console_.out() << "  Total: " << console_.paint(std::to_string(s.totalResources), Style::Highlight)
                       << "   Available: " << console_.paint(std::to_string(s.availableResources), Style::Success)
                       << "   Deployed: " << console_.paint(std::to_string(s.deployedResources), Style::Warning)
                       << "   Out of service: "
                       << console_.paint(std::to_string(s.outOfServiceResources), Style::Danger) << "\n\n";
        console_.printMenu({{"1", "Add resource"},
                            {"2", "View all resources"},
                            {"3", "Search resources"},
                            {"4", "Update availability / location"},
                            {"5", "Assign resource to emergency"},
                            {"6", "Release resource"},
                            {"7", "Assignment history"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            addResource();
        } else if (choice == "2") {
            viewResources();
        } else if (choice == "3") {
            searchResources();
        } else if (choice == "4") {
            updateResourceAvailability();
        } else if (choice == "5") {
            assignResourceMenu();
        } else if (choice == "6") {
            releaseResourceMenu();
        } else if (choice == "7") {
            viewAssignmentHistory();
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

void RESQApplication::addResource()
{
    ScreenScope scope(console_, "Add");
    console_.printHeader("Add a rescue resource");
    console_.info("Type :q at any prompt to cancel.");
    console_.out() << '\n';
    const std::string suggested = resources_.generateId();
    std::string id;
    while (true) {
        const std::string input = console_.readText("Resource ID [Enter = " + suggested + "]", 8, false);
        id = input.empty() ? suggested : util::toUpper(input);
        std::string error = Resource::validateId(id);
        if (error.empty() && resources_.exists(id)) {
            error = "A resource with ID " + id + " already exists. IDs must be unique.";
        }
        if (error.empty()) {
            break;
        }
        console_.error(error);
    }
    const ResourceType type = pickResourceType();
    const std::string location = readValidated("Current location (base)", Resource::kMaxLocation, Resource::validateLocation);
    const int capacity = static_cast<int>(console_.readNumberOr("Capacity - people it can carry or serve (0 = n/a)", 0,
                                                                Resource::kMaxCapacity, defaultCapacity(type)));
    console_.out() << "  " << console_.paint("Most resources serve one emergency at a time. Allow more only if this "
                                             "resource can genuinely be split (e.g. stock of supply kits).",
                                             Style::Muted)
                   << '\n';
    const int maxConcurrent = static_cast<int>(
        console_.readNumberOr("Max simultaneous emergencies (1 = exclusive)", 1, Resource::kMaxConcurrent, 1));

    Resource resource(id, type, location, capacity, maxConcurrent);
    console_.out() << '\n';
    printResourceCard(resource);
    if (!console_.confirm("Add this resource?", true)) {
        console_.warning("Not added. Nothing was saved.");
        console_.pause();
        return;
    }
    const std::string error = resources_.add(resource);
    if (!error.empty()) {
        console_.error(error);
    } else {
        persist();
        log("Added resource " + id + " (" + shortName(type) + ")");
        console_.success("Resource " + id + " added and available for assignment.");
    }
    console_.pause();
}

void RESQApplication::viewResources()
{
    ScreenScope scope(console_, "All resources");
    console_.printHeader("All rescue resources");
    if (resources_.empty()) {
        console_.info("No resources yet. Add one (Resources > 1) or load demo data (menu 8).");
    } else {
        printResourceTable(resources_.all());
    }
    console_.pause();
}

void RESQApplication::searchResources()
{
    ScreenScope scope(console_, "Search");
    console_.printHeader("Search resources");
    if (resources_.empty()) {
        console_.info("There are no resources to search yet.");
        console_.pause();
        return;
    }
    console_.printMenu({{"1", "By resource ID"},
                        {"2", "By type"},
                        {"3", "By location"},
                        {"4", "By availability"},
                        {"0", "Back"}});
    const std::string choice = console_.readChoice("Search by");
    std::vector<const Resource*> results;
    std::string description;
    if (choice == "0") {
        return;
    } else if (choice == "1" || choice == "3") {
        const std::string query = console_.readText("Search text", 40);
        results = resources_.search(choice == "1" ? ResourceSearchField::Id : ResourceSearchField::Location, query);
        description = "\"" + query + "\"";
    } else if (choice == "2") {
        const ResourceType type = pickResourceType();
        results = resources_.byType(type);
        description = "type " + toString(type);
    } else if (choice == "4") {
        console_.printMenu({{"1", "Available"}, {"2", "Deployed"}, {"3", "Out of service"}});
        const int n = static_cast<int>(console_.readNumber("Availability (1-3)", 1, 3));
        const ResourceStatus status = allResourceStatuses()[static_cast<size_t>(n - 1)];
        results = resources_.byStatus(status);
        description = "status " + toString(status);
    } else {
        console_.error("Invalid option.");
        console_.pause();
        return;
    }
    console_.out() << '\n';
    if (results.empty()) {
        console_.warning("No resources match " + description + ".");
    } else {
        console_.success(std::to_string(results.size()) + " match(es) for " + description + ".");
        if (results.size() == 1) {
            printResourceCard(*results.front());
        } else {
            printResourceTable(results);
        }
    }
    console_.pause();
}

void RESQApplication::updateResourceAvailability()
{
    ScreenScope scope(console_, "Update");
    console_.printHeader("Update resource availability");
    if (resources_.empty()) {
        console_.info("There are no resources yet.");
        console_.pause();
        return;
    }
    Resource* r = promptResource("Resource ID");
    if (r == nullptr) {
        console_.pause();
        return;
    }
    const std::string id = r->id();
    printResourceCard(*r);
    console_.printMenu({{"1", "Mark out of service"}, {"2", "Return to service"}, {"3", "Update current location"}, {"0", "Back"}});
    const std::string choice = console_.readChoice("Choose");
    std::string error;
    std::string change;
    if (choice == "0") {
        return;
    } else if (choice == "1") {
        error = resources_.setOutOfService(id, true);
        change = id + " marked out of service; it cannot be assigned until returned to service.";
    } else if (choice == "2") {
        if (!r->isOutOfService()) {
            console_.info(id + " is already in service.");
            console_.pause();
            return;
        }
        error = resources_.setOutOfService(id, false);
        change = id + " is back in service.";
    } else if (choice == "3") {
        const std::string location = readValidated("New location", Resource::kMaxLocation, Resource::validateLocation);
        error = resources_.setLocation(id, location);
        change = id + " location updated to " + location + ".";
    } else {
        error = "Invalid option.";
    }
    if (!error.empty()) {
        console_.error(error);
    } else {
        persist();
        log(change.substr(0, 40));
        console_.success(change);
    }
    console_.pause();
}

void RESQApplication::assignResourceMenu()
{
    ScreenScope scope(console_, "Assign");
    console_.printHeader("Assign a resource to an emergency");
    const std::vector<const Emergency*> queue = emergencies_.pendingByPriority(now());
    if (!queue.empty()) {
        console_.out() << "  Pending cases (highest priority first):\n";
        printEmergencyTable(queue);
        console_.out() << '\n';
    }
    Emergency* e = promptEmergency("Emergency ID");
    if (e != nullptr) {
        if (!e->isActive()) {
            console_.error("Case " + e->id() + " is " + toString(e->status()) + "; only active cases can receive resources.");
        } else {
            assignResourceFlow(e->id());
        }
    }
    console_.pause();
}

void RESQApplication::releaseResourceMenu()
{
    ScreenScope scope(console_, "Release");
    console_.printHeader("Release a resource");
    const std::vector<const Resource*> deployed = resources_.byStatus(ResourceStatus::Deployed);
    if (deployed.empty()) {
        console_.info("No resources are currently deployed.");
        console_.pause();
        return;
    }
    printResourceTable(deployed);
    console_.out() << '\n';
    Resource* r = promptResource("Resource ID to release");
    if (r == nullptr) {
        console_.pause();
        return;
    }
    const std::vector<std::string> assigned = r->assignedEmergencies();
    if (assigned.empty()) {
        console_.info(r->id() + " is not assigned to any emergency.");
        console_.pause();
        return;
    }
    std::string eid = assigned.front();
    if (assigned.size() > 1) {
        eid = util::toUpper(console_.readText("Release from which emergency? (" + util::trim(eid) + ", ...)", 16));
    }
    if (console_.confirm("Release " + r->id() + " from " + eid + "?")) {
        const OpResult result = dispatcher_.release(eid, r->id(), now());
        if (result.ok) {
            persist();
            log("Released " + r->id() + " from " + eid);
            console_.success(result.message);
        } else {
            console_.error(result.message);
        }
    }
    console_.pause();
}

void RESQApplication::viewAssignmentHistory()
{
    ScreenScope scope(console_, "History");
    console_.printHeader("Assignment history");
    const std::vector<AssignmentRecord>& history = resources_.history();
    if (history.empty()) {
        console_.info("No assignments have been recorded yet.");
        console_.pause();
        return;
    }
    console_.out() << "  Latest " << std::min<size_t>(history.size(), 15) << " of " << history.size()
                   << " record(s), newest first:\n\n";
    Table table({{"#", 4, true}, {"Case", 9}, {"Resource", 9}, {"Assigned", 11}, {"Released", 11}, {"Duration", 8},
                 {"Note", 19}});
    const std::time_t t = now();
    for (size_t shown = 0; shown < history.size() && shown < 15; ++shown) {
        const AssignmentRecord& record = history[history.size() - 1 - shown];
        const std::time_t end = record.isActive() ? t : record.releasedAt;
        table.addRow({{std::to_string(record.id), Style::Muted},
                      {record.emergencyId, Style::Bold},
                      {record.resourceId},
                      {util::formatCompact(record.assignedAt)},
                      {record.isActive() ? "active" : util::formatCompact(record.releasedAt),
                       record.isActive() ? Style::Warning : Style::Plain},
                      {util::formatDuration(end - record.assignedAt)},
                      {record.note, Style::Muted}});
    }
    table.print(console_);
    console_.pause();
}

void RESQApplication::reportsMenu()
{
    ScreenScope scope(console_, "Reports");
    while (true) {
        console_.printHeader("Reports & analytics");
        console_.info("All figures are calculated from the records stored in \"" + files_.dataDirectory() + "\".");
        console_.out() << '\n';
        console_.printMenu({{"1", "Overview & case status"},
                            {"2", "By severity & category"},
                            {"3", "Resource utilization"},
                            {"4", "Response & resolution times"},
                            {"5", "Full report"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        }
        console_.printHeader("Reports & analytics");
        if (choice == "1") {
            reportOverview();
        } else if (choice == "2") {
            reportBreakdown();
        } else if (choice == "3") {
            reportResources();
        } else if (choice == "4") {
            reportTiming();
        } else if (choice == "5") {
            reportOverview();
            reportBreakdown();
            reportResources();
            reportTiming();
        } else {
            console_.error("Invalid option.");
        }
        console_.pause();
    }
}

void RESQApplication::reportOverview()
{
    const Statistics s = analytics_.compute();
    console_.printSection("Overview");
    const int resolved = s.count(Status::Resolved);
    const int open = s.activeCases;
    console_.out() << "  Total emergencies registered   : "
                   << console_.paint(std::to_string(s.totalEmergencies), Style::Highlight)
                   << "\n  Open (pending/assigned/active) : " << open
                   << "\n  Critical cases still open      : "
                   << console_.paint(std::to_string(s.activeCritical), Style::Critical)
                   << "\n  People affected in open cases  : " << s.peopleAffectedActive << '\n';
    if (s.totalEmergencies == 0) {
        console_.info("No emergencies recorded yet - charts appear once cases are registered.");
        return;
    }
    console_.printSection("Cases by status");
    int maxValue = 0;
    for (int v : s.byStatus) {
        maxValue = std::max(maxValue, v);
    }
    for (Status status : allStatuses()) {
        console_.printBar(toString(status), s.count(status), maxValue, statusStyle(status));
    }
    console_.printSection("Resolved versus pending");
    const int pending = s.count(Status::Pending);
    console_.printBar("Resolved", resolved, std::max(resolved, pending), Style::Success);
    console_.printBar("Pending", pending, std::max(resolved, pending), Style::Warning);
    console_.out() << "  " << resolved << " of " << s.totalEmergencies << " cases resolved ("
                   << percent(resolved, s.totalEmergencies) << "); " << open << " still open.\n";
}

void RESQApplication::reportBreakdown()
{
    const Statistics s = analytics_.compute();
    if (s.totalEmergencies == 0) {
        console_.info("No emergencies recorded yet.");
        return;
    }
    console_.printSection("Emergencies by severity");
    int maxSeverity = 0;
    for (int v : s.bySeverity) {
        maxSeverity = std::max(maxSeverity, v);
    }
    for (Severity severity : allSeverities()) {
        const Style style = severity == Severity::Low ? Style::Success : severityStyle(severity);
        console_.printBar(toString(severity), s.bySeverity[static_cast<size_t>(indexOf(severity))], maxSeverity, style);
    }
    console_.printSection("Emergencies by category");
    int maxCategory = 0;
    for (int v : s.byCategory) {
        maxCategory = std::max(maxCategory, v);
    }
    for (Category category : allCategories()) {
        console_.printBar(toString(category), s.byCategory[static_cast<size_t>(indexOf(category))], maxCategory,
                          Style::Accent);
    }
}

void RESQApplication::reportResources()
{
    const Statistics s = analytics_.compute();
    console_.printSection("Resource utilization");
    if (s.totalResources == 0) {
        console_.info("No resources recorded yet.");
        return;
    }
    const double utilization = s.utilizationPercent();
    console_.out() << "  " << s.totalResources << " resources: " << console_.paint(std::to_string(s.availableResources) + " available", Style::Success)
                   << ", " << console_.paint(std::to_string(s.deployedResources) + " deployed", Style::Warning) << ", "
                   << console_.paint(std::to_string(s.outOfServiceResources) + " out of service", Style::Danger) << "\n";
    if (utilization >= 0) {
        console_.printBar("Utilization %", static_cast<int>(std::lround(utilization)), 100, Style::Warning);
    }
    console_.out() << '\n';
    Table table({{"Resource type", 24}, {"Total", 5, true}, {"Available", 9, true}, {"Deployed", 8, true}, {"Out of svc", 10, true}});
    for (ResourceType type : allResourceTypes()) {
        const TypeUsage& u = s.byType[static_cast<size_t>(indexOf(type))];
        table.addRow({{toString(type)},
                      {std::to_string(u.total), Style::Highlight},
                      {std::to_string(u.available), u.available > 0 ? Style::Success : Style::Muted},
                      {std::to_string(u.deployed), u.deployed > 0 ? Style::Warning : Style::Muted},
                      {std::to_string(u.outOfService), u.outOfService > 0 ? Style::Danger : Style::Muted}});
    }
    table.print(console_);
    const auto top = analytics_.mostDeployed(5);
    if (!top.empty()) {
        console_.printSection("Most deployed resources (assignment history)");
        for (const auto& entry : top) {
            console_.printBar(entry.first, entry.second, top.front().second, Style::Accent);
        }
    }
}

void RESQApplication::reportTiming()
{
    const Statistics s = analytics_.compute();
    console_.printSection("Response & resolution times");
    if (s.responseSamples > 0) {
        console_.out() << "  Average time to first assignment : "
                       << console_.paint(util::formatDuration(static_cast<long long>(s.averageResponseSeconds)), Style::Highlight)
                       << "  (" << s.responseSamples << " case(s))\n";
    } else {
        console_.out() << "  Average time to first assignment : not enough data (no case assigned yet)\n";
    }
    if (s.resolutionSamples > 0) {
        console_.out() << "  Average time to resolution       : "
                       << console_.paint(util::formatDuration(static_cast<long long>(s.averageResolutionSeconds)), Style::Highlight)
                       << "  (" << s.resolutionSamples << " resolved case(s))\n";
    } else {
        console_.out() << "  Average time to resolution       : not enough data (no resolved case with valid times)\n";
    }
    console_.out() << "  "
                   << console_.paint("Times come from timestamps recorded in RESQ and depend on operators updating "
                                     "case status promptly.",
                                     Style::Muted)
                   << '\n';
}

void RESQApplication::demoMenu()
{
    ScreenScope scope(console_, "Demo data");
    while (true) {
        console_.printHeader("Demonstration data");
        console_.warning("Demo records are FICTIONAL. Their IDs start with E-D / R-D and are marked * in tables.");
        const size_t demoCases = emergencies_.demoIds().size();
        const size_t demoResources = resources_.demoIds().size();
        console_.out() << "  Demo records loaded: " << demoCases << " cases, " << demoResources << " resources.  "
                       << "Your own records: " << emergencies_.size() - demoCases << " cases, "
                       << resources_.size() - demoResources << " resources.\n\n";
        console_.printMenu({{"1", "Load demo data (keeps your data)"},
                            {"2", "Reset demo data to original"},
                            {"3", "Remove all demo data"},
                            {"4", "Erase ALL data (incl. yours)"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            if (DemoData::present(emergencies_, resources_)) {
                console_.warning("Demo data is already loaded. Use option 2 to restore it to its original state.");
            } else if ((emergencies_.empty() && resources_.empty()) ||
                       console_.confirm("Your existing records are kept and demo records are added beside them. Continue?")) {
                const DemoResult result = DemoData::load(emergencies_, resources_, dispatcher_, now());
                persist();
                log("Demo data loaded");
                console_.success("Added " + std::to_string(result.emergenciesAdded) + " fictional cases and " +
                                 std::to_string(result.resourcesAdded) + " fictional resources.");
                if (result.skipped > 0) {
                    console_.warning(std::to_string(result.skipped) + " demo record(s) skipped because the ID is already used by your data.");
                }
            }
        } else if (choice == "2") {
            if (console_.confirm("Remove every demo record (including changes made to them) and load a fresh copy? "
                                 "Your own records are not affected.")) {
                const int removed = DemoData::removeAll(emergencies_, resources_, dispatcher_, now());
                const DemoResult result = DemoData::load(emergencies_, resources_, dispatcher_, now());
                persist();
                log("Demo data reset");
                console_.success("Removed " + std::to_string(removed) + " demo record(s); loaded " +
                                 std::to_string(result.emergenciesAdded + result.resourcesAdded) + " fresh ones.");
            }
        } else if (choice == "3") {
            if (!DemoData::present(emergencies_, resources_)) {
                console_.info("No demo records are loaded.");
            } else if (console_.confirm("Remove all demo records? Your own records are kept; your cases that used a "
                                        "demo resource lose that assignment.")) {
                const int removed = DemoData::removeAll(emergencies_, resources_, dispatcher_, now());
                persist();
                log("Demo data removed");
                console_.success("Removed " + std::to_string(removed) + " demo record(s).");
            }
        } else if (choice == "4") {
            if (emergencies_.empty() && resources_.empty()) {
                console_.info("There is no data to erase.");
            } else {
                console_.warning("This erases ALL emergencies, resources and history, including your own records.");
                if (console_.confirmWord("Erase everything", "DELETE")) {
                    std::string error;
                    const std::vector<std::string> copies = files_.archiveAll("before-erase", error);
                    if (!error.empty()) {
                        console_.error(error + " Nothing was erased.");
                    } else {
                        emergencies_.clear();
                        resources_.clear();
                        persist();
                        log("All data erased");
                        console_.success("All data erased.");
                        for (const std::string& copy : copies) {
                            console_.info("Previous file archived as " + copy);
                        }
                    }
                } else {
                    console_.info("Confirmation word not entered - nothing was erased.");
                }
            }
        } else {
            console_.error("Invalid option.");
        }
        console_.pause();
    }
}

void RESQApplication::settingsMenu()
{
    ScreenScope scope(console_, "Settings");
    while (true) {
        console_.printHeader("Settings & data files");
        const auto onOff = [this](bool value) {
            return value ? console_.paint("ON", Style::Success) : console_.paint("OFF", Style::Muted);
        };
        console_.out() << "  Colors        : " << onOff(console_.color())
                       << (console_.ansi() ? "" : "  (terminal without ANSI support)") << '\n'
                       << "  Box drawing   : " << (console_.unicode() ? "Unicode" : "ASCII") << '\n'
                       << "  Animations    : " << onOff(console_.animations()) << '\n'
                       << "  Data folder   : " << files_.dataDirectory() << "\n\n";
        console_.printMenu({{"1", "Toggle colors"},
                            {"2", "Toggle Unicode / ASCII borders"},
                            {"3", "Toggle animations"},
                            {"4", "Save all data now"},
                            {"5", "Data file status & load report"},
                            {"6", "How priority is calculated"},
                            {"7", "About & disclaimer"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            if (!console_.ansi()) {
                console_.warning("This terminal does not appear to support ANSI colors (start with --color to force).");
                console_.pause();
            }
            console_.setColor(!console_.color());
        } else if (choice == "2") {
            console_.setUnicode(!console_.unicode());
        } else if (choice == "3") {
            console_.setAnimations(!console_.animations());
        } else if (choice == "4") {
            console_.spinner("Saving ...", 400);
            if (persist()) {
                console_.success("All data saved to " + files_.dataDirectory() + ".");
            }
            console_.pause();
        } else if (choice == "5") {
            showDataStatus();
        } else if (choice == "6") {
            showPriorityHelp();
        } else if (choice == "7") {
            showAbout();
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

void RESQApplication::showDataStatus()
{
    ScreenScope scope(console_, "Data files");
    console_.printHeader("Data file status");
    const auto describe = [](bool found) { return found ? "found at start-up" : "not found at start-up (created on save)"; };
    console_.printBox("FILES",
                      {"emergencies.txt : " + std::string(describe(loadReport_.emergenciesFileFound)),
                       "resources.txt   : " + std::string(describe(loadReport_.resourcesFileFound)),
                       "assignments.txt : " + std::string(describe(loadReport_.assignmentsFileFound)),
                       "Folder          : " + files_.dataDirectory(),
                       "Loaded          : " + std::to_string(loadReport_.emergenciesLoaded) + " emergencies, " +
                           std::to_string(loadReport_.resourcesLoaded) + " resources, " +
                           std::to_string(loadReport_.assignmentsLoaded) + " history records",
                       "Last save       : " + (lastSaved_ > 0 ? util::formatDateTime(lastSaved_) : std::string("-")),
                       "Safety          : previous version kept as <file>.bak on every save"});
    if (loadReport_.warnings.empty() && reconcileNotes_.empty()) {
        console_.success("No problems were found when the files were loaded.");
    }
    for (const std::string& warning : loadReport_.warnings) {
        console_.warning(warning);
    }
    for (const std::string& note : reconcileNotes_) {
        console_.warning("Data check: " + note);
    }
    for (const std::string& path : loadReport_.preserved) {
        console_.info("Original kept as " + path);
    }
    console_.pause();
}

void RESQApplication::showPriorityHelp()
{
    ScreenScope scope(console_, "Priority");
    console_.printHeader("How the priority score is calculated");
    console_.printBox("PRIORITY SCORING RULES", PriorityCalculator::explanation());
    console_.pause();
}

void RESQApplication::showAbout()
{
    ScreenScope scope(console_, "About");
    console_.printHeader("About RESQ");
    console_.printBox(
        "RESQ " + std::string(kVersion),
        {"Rescue and Emergency Support System - an academic C/C++ prototype.",
         "",
         console_.paint("IMPORTANT DISCLAIMER", Style::Critical),
         "- RESQ is NOT connected to any real emergency service.",
         "- It does not predict disasters or risk. Priority scores only order",
         "  the queue using fixed, illustrative rules.",
         "- It has no live locations or real-time resource availability. All",
         "  data is what operators type in, or the fictional demo records.",
         "- Human operators remain responsible for every decision.",
         "",
         "Privacy: names and contact details are obfuscated in the data files",
         "so they are not readable at a glance. This is NOT encryption - protect",
         "the data folder with operating-system permissions."},
        Style::Danger);
    console_.pause();
}

void RESQApplication::showHelp()
{
    ScreenScope scope(console_, "Help");
    console_.printHeader("Help - quick guide");
    console_.printBox("TYPICAL WORKFLOW",
                      {"1. Register each incoming emergency (menu 1).",
                       "2. Open Dispatch (menu 5) > Process next: RESQ shows the",
                       "   highest-priority pending case and suitable resources.",
                       "3. Assign one or more resources - the case becomes Assigned.",
                       "4. When crews start work, mark the case In Progress.",
                       "5. When the operation is complete, mark it Resolved -",
                       "   its resources are released automatically.",
                       "",
                       "Tips: type :q in any form to cancel it. Tables mark fictional",
                       "demo records with *. Data is saved after every change."});
    console_.out() << '\n';
    console_.printBox("PRIORITY SCORE", PriorityCalculator::explanation());
    console_.out() << "\n  " << console_.paint("RESQ is an academic prototype and is not connected to real emergency "
                                               "services.",
                                               Style::Muted)
                   << '\n';
    console_.pause();
}

}
