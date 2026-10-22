#include "app.h"

#include "resq_c.h"

#include <algorithm>

namespace resq {

namespace {

Color severityColor(Severity s) {
    return s == Severity::Critical ? Color::Red : (s == Severity::Low ? Color::None : Color::Yellow);
}

Color statusColor(Status s) {
    static const Color colors[] = {Color::Yellow, Color::Cyan, Color::Magenta, Color::Green, Color::Gray};
    return colors[idx(s)];
}

std::string resourceNames(const std::vector<const Resource*>& list) {
    std::string text;
    for (const Resource* r : list) text += (text.empty() ? "" : ", ") + r->id() + " (" + name(r->type()) + ")";
    return text.empty() ? "none" : text;
}

}

RESQApplication::RESQApplication(const std::string& folder, bool color)
    : console_(std::cin, std::cout, color, resq_is_interactive() == 1), files_(folder) {}

template <typename E> E RESQApplication::pick(const std::string& label, int count) {
    std::vector<std::string> items;
    for (int i = 1; i <= count; ++i) items.push_back("[" + std::to_string(i) + "] " + name(static_cast<E>(i)));
    console_.out << "  " << label << ":\n";
    console_.menu(items);
    return static_cast<E>(console_.number(label + " (1-" + std::to_string(count) + ")", 1, count));
}

bool RESQApplication::save() {
    std::string error;
    if (files_.save(emergencies_, resources_, error)) return true;
    console_.error("Could not save: " + error);
    return false;
}

int RESQApplication::run() {
    bool saved = false;
    try {
        console_.clear();
        console_.logo();
        const std::vector<std::string> warnings = files_.load(emergencies_, resources_, now());
        if (files_.foundData()) {
            console_.ok("Loaded " + std::to_string(emergencies_.size()) + " emergencies and " +
                        std::to_string(resources_.size()) + " resources from \"" + files_.folder() + "\".");
        } else {
            console_.warn("No saved data found - starting empty. Use menu 8 to load demo data.");
        }
        for (const std::string& warning : warnings) console_.warn(warning);
        console_.pause();
        while (true) {
            dashboard();
            const std::string c = console_.choice("Select an option");
            if (c == "0") break;
            try {
                if (c == "1")
                    registerEmergency();
                else if (c == "2")
                    viewEmergencies();
                else if (c == "3")
                    searchEmergencies();
                else if (c == "4")
                    updateEmergency();
                else if (c == "5")
                    dispatchMenu();
                else if (c == "6")
                    resourceMenu();
                else if (c == "7")
                    reports();
                else if (c == "8")
                    demoMenu();
                else {
                    console_.error("\"" + c + "\" is not a menu option.");
                    console_.pause();
                }
            } catch (const InputCancelled&) {
                console_.warn("Cancelled.");
                console_.pause();
            }
        }
    } catch (const InputClosed&) {
        console_.warn("Input closed - saving and exiting.");
    }
    saved = save();
    if (saved) console_.ok("All records saved to \"" + files_.folder() + "\".");
    console_.box("RESQ CLOSED",
                 {"Thank you for using RESQ. Stay safe.", "In a real emergency, contact your local emergency number."},
                 Color::Red);
    return saved ? 0 : 1;
}

void RESQApplication::dashboard() {
    console_.clear();
    console_.logo();
    const Stats s = AnalyticsManager::compute(emergencies_, resources_);
    const auto row = [&](const std::string& left, int a, Color ca, const std::string& right, int b, Color cb) {
        return console_.paint(Console::pad(left + std::to_string(a), 36), ca) +
               console_.paint(right + std::to_string(b), cb);
    };
    console_.box(
        "DASHBOARD",
        {
            row("Total cases        : ", s.total, Color::Bold, "Total resources    : ", s.resources, Color::Bold),
            row("Pending            : ", s.count(Status::Pending), Color::Yellow, "Available          : ", s.available,
                Color::Green),
            row("Assigned           : ", s.count(Status::Assigned), Color::Cyan, "Deployed           : ", s.deployed,
                Color::Yellow),
            row("In progress        : ", s.count(Status::InProgress), Color::Magenta,
                "Out of service     : ", s.outOfService, Color::Red),
            row("Resolved           : ", s.count(Status::Resolved), Color::Green,
                "Cancelled cases    : ", s.count(Status::Cancelled), Color::Gray),
            row("Critical (open)    : ", s.activeCritical, Color::Red, "Open cases         : ", s.open(), Color::Bold),
        });
    const std::vector<const Emergency*> queue = emergencies_.queue(now());
    if (!queue.empty()) {
        const Emergency* e = queue.front();
        console_.out << "  Next in queue: "
                     << console_.paint(e->id() + " " + name(e->severity()), severityColor(e->severity())) << " | "
                     << name(e->category()) << " | " << e->location() << " | score " << e->score() << "\n\n";
    } else {
        console_.out << "  " << console_.paint("Queue clear: no pending emergencies.", Color::Green) << "\n\n";
    }
    console_.menu({"[1] Register emergency", "[5] Dispatch & operations", "[2] View & sort emergencies",
                   "[6] Resource management", "[3] Search emergencies", "[7] Reports & analytics",
                   "[4] Update emergency", "[8] Demo data", "", "[0] Save & exit"});
}

void RESQApplication::showCase(const Emergency& e) {
    const Priority p = e.priority(now());
    std::vector<std::string> lines{
        "Category        : " + name(e.category()),
        "Severity        : " + console_.paint(name(e.severity()), severityColor(e.severity())) +
            "     Priority score: " + std::to_string(e.score()),
        "Status          : " + console_.paint(name(e.status()), statusColor(e.status())),
        "Location        : " + e.location(),
        "Reported by     : " + e.reporter() + "  (" + e.contact() + ")",
        "People affected : " + std::to_string(e.affected()),
        "Registered      : " + util::timeText(e.registered()) +
            (e.status() == Status::Pending ? "  (waiting " + util::duration(now() - e.registered()) + ")" : ""),
    };
    if (e.assignedAt() > 0) lines.push_back("Assigned        : " + util::timeText(e.assignedAt()));
    if (e.startedAt() > 0) lines.push_back("Work started    : " + util::timeText(e.startedAt()));
    if (e.closedAt() > 0) lines.push_back("Closed          : " + util::timeText(e.closedAt()));
    for (size_t i = 0; i < e.description().size(); i += 54)
        lines.push_back((i == 0 ? "Description     : " : "                  ") + e.description().substr(i, 54));
    lines.push_back("Resources       : " + resourceNames(resources_.assignedTo(e.id())));
    console_.box("EMERGENCY " + e.id() + (e.demo() ? " [FICTIONAL DEMO]" : ""), lines,
                 e.severity() == Severity::Critical && e.active() ? Color::Red : Color::Cyan);
    console_.out << "  Score " << p.total() << " = severity " << p.severity << " + people " << p.people << " + waiting "
                 << p.waiting << " (" << p.minutes << " min) + category " << p.category << '\n'
                 << "  " << console_.paint("The score only orders the queue; it is not a risk prediction.", Color::Gray)
                 << '\n';
}

void RESQApplication::emergencyTable(const std::vector<const Emergency*>& list) {
    console_.out << "  "
                 << console_.paint(Console::pad("ID", 9) + Console::pad("Category", 18) + Console::pad("Location", 18) +
                                       Console::pad("Severity", 9) + Console::pad("Score", 6) + "Status",
                                   Color::Cyan)
                 << '\n';
    if (list.empty()) console_.out << "  (no records)\n";
    for (const Emergency* e : list) {
        console_.out << "  " << Console::pad(e->id() + (e->demo() ? "*" : ""), 9)
                     << Console::pad(name(e->category()), 18) << Console::pad(e->location(), 18)
                     << console_.paint(Console::pad(name(e->severity()), 9), severityColor(e->severity()))
                     << Console::pad(std::to_string(e->score()), 6)
                     << console_.paint(name(e->status()), statusColor(e->status())) << '\n';
    }
}

void RESQApplication::resourceTable(const std::vector<const Resource*>& list) {
    console_.out << "  "
                 << console_.paint(Console::pad("#", 4) + Console::pad("ID", 8) + Console::pad("Type", 16) +
                                       Console::pad("Location", 18) + Console::pad("Cap", 5) + "Status",
                                   Color::Cyan)
                 << '\n';
    if (list.empty()) console_.out << "  (no records)\n";
    for (size_t i = 0; i < list.size(); ++i) {
        const Resource* r = list[i];
        std::string status = r->statusText();
        for (const std::string& id : r->assigned()) status += " " + id;
        console_.out << "  " << Console::pad(std::to_string(i + 1), 4)
                     << Console::pad(r->id() + (r->demo() ? "*" : ""), 8) << Console::pad(name(r->type()), 16)
                     << Console::pad(r->location(), 18) << Console::pad(std::to_string(r->capacity()), 5)
                     << console_.paint(status, r->outOfService()
                                                   ? Color::Red
                                                   : (r->assigned().empty() ? Color::Green : Color::Yellow))
                     << '\n';
    }
}

Emergency* RESQApplication::askEmergency(const std::string& prompt) {
    Emergency* e = emergencies_.find(console_.text(prompt, 8));
    if (e == nullptr) console_.error("No emergency with that ID.");
    return e;
}

Resource* RESQApplication::askResource(const std::string& prompt) {
    Resource* r = resources_.find(console_.text(prompt, 8));
    if (r == nullptr) console_.error("No resource with that ID.");
    return r;
}

void RESQApplication::registerEmergency() {
    console_.title("Register emergency");
    console_.out << "  Type :q at any prompt to cancel.\n";
    std::string id;
    while (true) {
        const std::string next = emergencies_.nextId();
        id = util::upper(console_.text("Emergency ID [Enter = " + next + "]", 8, false));
        id = id.empty() ? next : id;
        if (!util::validId(id))
            console_.error("ID must be 3-8 letters, digits or '-', starting with a letter.");
        else if (emergencies_.find(id) != nullptr)
            console_.error("An emergency with ID " + id + " already exists. IDs must be unique.");
        else
            break;
    }
    const std::string reporter = console_.text("Reporter name", 40);
    const Category category = pick<Category>("Category", kCategories);
    const std::string location = console_.text("Location", 40);
    std::string contact = console_.text("Contact phone or e-mail", 32);
    while (!Emergency::checkContact(contact).empty()) {
        console_.error(Emergency::checkContact(contact));
        contact = console_.text("Contact phone or e-mail", 32);
    }
    const std::string description = console_.text("Description", 200);
    const Severity severity = pick<Severity>("Severity", kSeverities);
    const int affected = static_cast<int>(console_.number("People affected", 0, 1000000));
    std::time_t registered = now();
    while (true) {
        const std::string when = console_.text("Registration time [Enter = now, or YYYY-MM-DD HH:MM]", 16, false);
        if (when.empty()) break;
        if (util::parseTime(when, registered) && registered <= now()) break;
        console_.error("Use YYYY-MM-DD HH:MM, not in the future.");
        registered = now();
    }
    if (!emergencies_
             .add(Emergency(id, reporter, category, location, contact, description, severity, affected, registered))
             .empty()) {
        return;
    }
    Emergency* e = emergencies_.find(id);
    const std::vector<const Emergency*> queue = emergencies_.queue(now());
    const auto position = std::find(queue.begin(), queue.end(), e) - queue.begin() + 1;
    save();
    console_.ok("Emergency " + id + " registered. Priority score " + std::to_string(e->score()) + ", position " +
                std::to_string(position) + " of " + std::to_string(queue.size()) + " in the queue.");
    if (severity == Severity::Critical) console_.warn("CRITICAL case: dispatch resources as soon as possible.");
    if (console_.confirm("Assign a resource now?")) assignFlow(*e);
    console_.pause();
}

void RESQApplication::viewEmergencies() {
    SortBy sortBy = SortBy::Priority;
    while (true) {
        console_.title("All emergencies");
        emergencies_.refresh(now());
        emergencyTable(emergencies_.sorted(sortBy));
        console_.out << '\n';
        console_.menu({"[1] Sort by severity", "[2] Sort by priority score", "[3] Sort by registration time",
                       "[4] Sort by ID", "[D] Show details of a case", "[0] Back"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        if (c == "1")
            sortBy = SortBy::Severity;
        else if (c == "2")
            sortBy = SortBy::Priority;
        else if (c == "3")
            sortBy = SortBy::Time;
        else if (c == "4")
            sortBy = SortBy::Id;
        else if (c == "D") {
            if (const Emergency* e = askEmergency("Emergency ID")) showCase(*e);
            console_.pause();
        }
    }
}

void RESQApplication::searchEmergencies() {
    console_.title("Search emergencies");
    console_.menu({"[1] By ID", "[2] By reporter name", "[3] By location", "[4] By category"});
    const int field = static_cast<int>(console_.number("Search by", 1, 4));
    const std::vector<const Emergency*> results = field == 4
                                                      ? emergencies_.byCategory(pick<Category>("Category", kCategories))
                                                      : emergencies_.search(field, console_.text("Search text", 40));
    if (results.empty()) {
        console_.warn("No emergencies found.");
    } else if (results.size() <= 5) {
        console_.ok(std::to_string(results.size()) + " match(es) found.");
        for (const Emergency* e : results) showCase(*e);
    } else {
        console_.ok(std::to_string(results.size()) + " matches found.");
        emergencyTable(results);
    }
    console_.pause();
}

void RESQApplication::updateEmergency() {
    console_.title("Update emergency");
    Emergency* e = askEmergency("Emergency ID");
    while (e != nullptr) {
        e->refresh(now());
        showCase(*e);
        console_.menu({"[1] Change severity", "[2] Update location", "[3] Edit description",
                       "[4] Update people affected", "[5] Change status", "[0] Done"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        try {
            if (c == "1")
                e->setSeverity(pick<Severity>("New severity", kSeverities));
            else if (c == "2")
                e->setLocation(console_.text("New location", 40));
            else if (c == "3")
                e->setDescription(console_.text("New description", 200));
            else if (c == "4")
                e->setAffected(static_cast<int>(console_.number("People affected", 0, 1000000)));
            else if (c == "5")
                changeStatus(*e);
            else
                console_.error("Invalid option.");
            if (c == "1" || c == "2" || c == "3" || c == "4") {
                e->refresh(now());
                save();
                console_.ok("Updated. New priority score: " + std::to_string(e->score()));
            }
        } catch (const std::exception& ex) {
            console_.error(ex.what());
        }
        console_.pause();
    }
    console_.pause();
}

void RESQApplication::changeStatus(Emergency& e) {
    std::vector<Status> allowed;
    for (int i = 1; i <= kStatuses; ++i) {
        if (Emergency::canMove(e.status(), static_cast<Status>(i))) {
            allowed.push_back(static_cast<Status>(i));
            console_.out << "  [" << allowed.size() << "] " << name(static_cast<Status>(i)) << '\n';
        }
    }
    if (allowed.empty()) {
        console_.warn("This case is closed; its status cannot change.");
        return;
    }
    const Status to =
        allowed[static_cast<size_t>(console_.number("New status", 1, static_cast<long long>(allowed.size())) - 1)];
    if (to == Status::Assigned) {
        assignFlow(e);
        return;
    }
    if (to != Status::InProgress &&
        !console_.confirm("Change to " + name(to) + "? Assigned resources will be released.")) {
        return;
    }
    const std::string error = resources_.changeStatus(e, to, now());
    if (error.empty()) {
        save();
        console_.ok("Case " + e.id() + " is now " + name(to) + ".");
    } else {
        console_.error(error);
    }
}

void RESQApplication::dispatchMenu() {
    while (true) {
        console_.title("Dispatch & operations");
        console_.menu({"[1] Process next highest-priority case", "[2] Manage active operations", "[0] Back"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        if (c == "1")
            processNext();
        else if (c == "2")
            activeOperations();
    }
}

void RESQApplication::processNext() {
    while (true) {
        const std::vector<const Emergency*> queue = emergencies_.queue(now());
        console_.title("Process next emergency");
        if (queue.empty()) {
            console_.ok("The pending queue is empty.");
            console_.pause();
            return;
        }
        Emergency* e = emergencies_.find(queue.front()->id());
        console_.out << "  Highest-priority pending case (" << queue.size() << " waiting):\n";
        showCase(*e);
        console_.menu({"[1] Assign resource(s)", "[2] Cancel case (false alarm)", "[0] Back"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        if (c == "1") {
            assignFlow(*e);
            if (e->status() == Status::Assigned && console_.confirm("Have crews started work? Mark In Progress?")) {
                resources_.changeStatus(*e, Status::InProgress, now());
                save();
                console_.ok("Case " + e->id() + " is now In Progress.");
            }
            console_.pause();
        } else if (c == "2" && console_.confirm("Cancel " + e->id() + "?")) {
            resources_.changeStatus(*e, Status::Cancelled, now());
            save();
        }
    }
}

void RESQApplication::assignFlow(Emergency& e) {
    while (e.active()) {
        const std::vector<const Resource*> suitable = resources_.suitableFor(e);
        console_.out << "\n  Suitable available resources for " << e.id() << " (" << name(e.category()) << ", "
                     << e.affected() << " people):\n";
        resourceTable(suitable);
        const std::string input = console_.text("Resource ID or # (Enter = done)", 8, false);
        if (input.empty()) return;
        long long number = 0;
        const std::string id =
            util::toInt(input, number) && number >= 1 && number <= static_cast<long long>(suitable.size())
                ? suitable[static_cast<size_t>(number - 1)]->id()
                : input;
        const std::string error = resources_.assign(e, id, now());
        if (!error.empty()) {
            console_.error(error);
            continue;
        }
        save();
        console_.ok("Resource " + util::upper(id) + " assigned to " + e.id() + ". Status: " + name(e.status()) + ".");
        if (!console_.confirm("Assign another resource?")) return;
    }
}

void RESQApplication::activeOperations() {
    while (true) {
        console_.title("Active operations");
        std::vector<const Emergency*> active;
        for (const Emergency* e : emergencies_.sorted(SortBy::Priority))
            if (e->status() == Status::Assigned || e->status() == Status::InProgress) active.push_back(e);
        emergencyTable(active);
        const std::string id = console_.text("Case ID to manage (Enter = back)", 8, false);
        if (id.empty()) return;
        Emergency* e = emergencies_.find(id);
        if (e == nullptr || !e->active()) {
            console_.error("That is not an active case.");
            console_.pause();
        } else {
            manageOperation(*e);
        }
    }
}

void RESQApplication::manageOperation(Emergency& e) {
    while (e.active()) {
        console_.title("Operation " + e.id());
        showCase(e);
        console_.menu({"[1] Assign another resource", "[2] Release a resource", "[3] Mark In Progress",
                       "[4] Mark Resolved", "[5] Cancel case", "[0] Back"});
        const std::string c = console_.choice("Choose");
        std::string error;
        if (c == "0") return;
        if (c == "1") {
            assignFlow(e);
            continue;
        }
        if (c == "2")
            error = resources_.release(e, console_.text("Resource ID", 8), now());
        else if (c == "3")
            error = resources_.changeStatus(e, Status::InProgress, now());
        else if (c == "4")
            error = console_.confirm("Is the operation complete?") ? resources_.changeStatus(e, Status::Resolved, now())
                                                                   : "Not resolved.";
        else if (c == "5")
            error = console_.confirm("Cancel this case?") ? resources_.changeStatus(e, Status::Cancelled, now())
                                                          : "Not cancelled.";
        else
            error = "Invalid option.";
        if (error.empty()) {
            save();
            console_.ok("Done. Case " + e.id() + " is " + name(e.status()) + ".");
        } else {
            console_.error(error);
        }
        console_.pause();
    }
}

void RESQApplication::resourceMenu() {
    while (true) {
        console_.title("Resource management");
        console_.menu({"[1] Add resource", "[2] View all resources", "[3] Search resources",
                       "[4] Change availability / location", "[5] Assign resource to emergency", "[6] Release resource",
                       "[0] Back"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        if (c == "1")
            addResource();
        else if (c == "2") {
            resourceTable(resources_.list());
            console_.pause();
        } else if (c == "3")
            searchResources();
        else if (c == "4")
            updateResource();
        else if (c == "5") {
            Emergency* e = askEmergency("Emergency ID");
            if (e != nullptr && e->active())
                assignFlow(*e);
            else if (e != nullptr)
                console_.error("Only open cases can receive resources.");
            console_.pause();
        } else if (c == "6")
            releaseResource();
    }
}

void RESQApplication::addResource() {
    static const int defaultCapacity[] = {2, 10, 6, 8, 20, 25};
    console_.title("Add resource");
    std::string id;
    while (true) {
        id = util::upper(console_.text("Resource ID [Enter = " + resources_.nextId() + "]", 8, false));
        id = id.empty() ? resources_.nextId() : id;
        if (!util::validId(id))
            console_.error("ID must be 3-8 letters, digits or '-', starting with a letter.");
        else if (resources_.find(id) != nullptr)
            console_.error("A resource with ID " + id + " already exists.");
        else
            break;
    }
    const ResourceType type = pick<ResourceType>("Type", kResourceTypes);
    const std::string location = console_.text("Current location", 40);
    const int capacity = static_cast<int>(console_.numberOr("Capacity (people)", 0, 10000, defaultCapacity[idx(type)]));
    const int shared = static_cast<int>(console_.numberOr("Max simultaneous emergencies (1 = exclusive)", 1, 20, 1));
    resources_.add(Resource(id, type, location, capacity, shared));
    save();
    console_.ok("Resource " + id + " added and available.");
    console_.pause();
}

void RESQApplication::searchResources() {
    console_.menu({"[1] By ID", "[2] By type", "[3] By location", "[4] By status (available/deployed/out)"});
    const int field = static_cast<int>(console_.number("Search by", 1, 4));
    const std::vector<const Resource*> results = resources_.search(field, console_.text("Search text", 40));
    if (results.empty())
        console_.warn("No resources found.");
    else
        resourceTable(results);
    console_.pause();
}

void RESQApplication::updateResource() {
    Resource* r = askResource("Resource ID");
    if (r != nullptr) {
        console_.menu({"[1] Mark out of service", "[2] Return to service", "[3] Change location"});
        const int c = static_cast<int>(console_.number("Choose", 1, 3));
        std::string error;
        if (c == 3) {
            try {
                r->setLocation(console_.text("New location", 40));
            } catch (const std::exception& ex) {
                error = ex.what();
            }
        } else {
            error = r->setOutOfService(c == 1);
        }
        if (error.empty()) {
            save();
            console_.ok(r->id() + " is now: " + r->statusText() + " at " + r->location() + ".");
        } else {
            console_.error(error);
        }
    }
    console_.pause();
}

void RESQApplication::releaseResource() {
    Resource* r = askResource("Resource ID");
    if (r != nullptr && r->assigned().empty()) {
        console_.warn(r->id() + " is not assigned to any emergency.");
    } else if (r != nullptr) {
        const std::string eid = r->assigned().size() == 1 ? r->assigned().front() : console_.text("Emergency ID", 8);
        Emergency* e = emergencies_.find(eid);
        const std::string error = e == nullptr ? "No emergency with that ID." : resources_.release(*e, r->id(), now());
        if (error.empty()) {
            save();
            console_.ok(r->id() + " released and available again.");
        } else {
            console_.error(error);
        }
    }
    console_.pause();
}

void RESQApplication::reports() {
    console_.title("Reports & analytics");
    const Stats s = AnalyticsManager::compute(emergencies_, resources_);
    console_.out << "  Calculated from the records stored in \"" << files_.folder() << "\".\n"
                 << "  Total emergencies: " << s.total << "   Open: " << s.open()
                 << "   Critical still open: " << s.activeCritical << "\n\n  BY STATUS\n";
    const int maxStatus = *std::max_element(s.status, s.status + kStatuses);
    for (int i = 1; i <= kStatuses; ++i)
        console_.bar(name(static_cast<Status>(i)), s.status[i - 1], maxStatus, statusColor(static_cast<Status>(i)));
    console_.out << "\n  BY SEVERITY\n";
    const int maxSeverity = *std::max_element(s.severity, s.severity + kSeverities);
    for (int i = 1; i <= kSeverities; ++i) {
        console_.bar(name(static_cast<Severity>(i)), s.severity[i - 1], maxSeverity,
                     severityColor(static_cast<Severity>(i)));
    }
    console_.out << "\n  BY CATEGORY\n";
    const int maxCategory = *std::max_element(s.category, s.category + kCategories);
    for (int i = 1; i <= kCategories; ++i)
        console_.bar(name(static_cast<Category>(i)), s.category[i - 1], maxCategory, Color::Cyan);
    console_.out << "\n  RESOLVED VS PENDING\n";
    const int resolved = s.count(Status::Resolved);
    const int pending = s.count(Status::Pending);
    console_.bar("Resolved", resolved, std::max(resolved, pending), Color::Green);
    console_.bar("Pending", pending, std::max(resolved, pending), Color::Yellow);
    const int inService = s.resources - s.outOfService;
    console_.out << "\n  RESOURCES: " << s.available << " available, " << s.deployed << " deployed, " << s.outOfService
                 << " out of service. Utilization: " << (inService > 0 ? s.deployed * 100 / inService : 0) << "%\n"
                 << "  Average time to first assignment: "
                 << (s.responseCount > 0 ? util::duration(static_cast<long long>(s.averageResponse)) : "no data")
                 << '\n'
                 << "  Average time to resolution      : "
                 << (s.resolvedCount > 0 ? util::duration(static_cast<long long>(s.averageResolution)) : "no data")
                 << '\n';
    console_.pause();
}

void RESQApplication::demoMenu() {
    console_.title("Demo data (fictional)");
    console_.menu({"[1] Load demo data", "[2] Reset demo data", "[3] Remove demo data", "[0] Back"});
    const std::string c = console_.choice("Choose");
    const bool hasData = emergencies_.size() + resources_.size() > 0;
    if (c == "1" && (!hasData || console_.confirm("Your records are kept; demo records are added. Continue?"))) {
        loadDemo();
    } else if (c == "2" && console_.confirm("Remove demo records and load a fresh copy? Your records are kept.")) {
        resources_.removeDemo(emergencies_, now());
        loadDemo();
    } else if (c == "3" && console_.confirm("Remove all demo records? Your records are kept.")) {
        resources_.removeDemo(emergencies_, now());
        save();
        console_.ok("Demo records removed.");
    }
    console_.pause();
}

void RESQApplication::loadDemo() {
    struct Case {
        const char* id;
        const char* who;
        Category c;
        const char* where;
        Severity s;
        int people;
        int minutesAgo;
        const char* what;
    };
    static const Case cases[] = {
        {"E-D01", "Riya Sharma", Category::Fire, "Old Market Square", Severity::Critical, 12, 25,
         "Shop fire, smoke on upper floor"},
        {"E-D02", "Arjun Mehta", Category::Medical, "Riverside Ward 3", Severity::High, 1, 40,
         "Elderly resident with chest pain"},
        {"E-D03", "Kavya Rao", Category::RoadAccident, "Hill Road Km 12", Severity::High, 4, 15,
         "Two-car collision, one trapped"},
        {"E-D04", "Imran Khan", Category::Flood, "Lakeview Colony", Severity::Medium, 30, 90,
         "Water entering ground-floor homes"},
        {"E-D05", "Neha Joshi", Category::Other, "Central Park Gate", Severity::Low, 0, 10,
         "Fallen tree blocking footpath"},
        {"E-D06", "Vikram Singh", Category::NaturalDisaster, "Northridge Village", Severity::Critical, 60, 180,
         "Landslide damaged houses"},
        {"E-D07", "Ananya Gupta", Category::Medical, "Sector 9 Housing", Severity::Medium, 1, 300,
         "Child with high fever"},
    };
    struct Unit {
        const char* id;
        ResourceType t;
        const char* where;
        int capacity;
        bool out;
    };
    static const Unit units[] = {
        {"R-D01", ResourceType::Ambulance, "Riverside", 2, false},
        {"R-D02", ResourceType::Ambulance, "Hill Road Depot", 2, true},
        {"R-D03", ResourceType::MedicalTeam, "District Hospital", 10, false},
        {"R-D04", ResourceType::FireEngine, "Old Market Station", 6, false},
        {"R-D05", ResourceType::RescueTeam, "Northridge", 8, false},
        {"R-D06", ResourceType::RescueTeam, "Central Depot", 8, false},
        {"R-D07", ResourceType::ReliefVehicle, "Central Depot", 20, false},
        {"R-D08", ResourceType::SupplyKit, "Central Store", 25, false},
    };
    const std::time_t t = now();
    int added = 0;
    for (const Unit& u : units) {
        Resource r(u.id, u.t, u.where, u.capacity, u.t == ResourceType::SupplyKit ? 3 : 1, true);
        r.setOutOfService(u.out);
        added += resources_.add(r).empty() ? 1 : 0;
    }
    for (const Case& c : cases) {
        added +=
            emergencies_
                    .add(Emergency(c.id, std::string(c.who) + " (demo)", c.c, c.where, "000-555-0100",
                                   std::string("[FICTIONAL] ") + c.what, c.s, c.people, t - c.minutesAgo * 60, true))
                    .empty()
                ? 1
                : 0;
    }
    Emergency* landslide = emergencies_.find("E-D06");
    if (landslide != nullptr && landslide->status() == Status::Pending &&
        resources_.assign(*landslide, "R-D05", t - 170 * 60).empty()) {
        resources_.assign(*landslide, "R-D07", t - 165 * 60);
        resources_.changeStatus(*landslide, Status::InProgress, t - 150 * 60);
    }
    Emergency* fever = emergencies_.find("E-D07");
    if (fever != nullptr && fever->status() == Status::Pending &&
        resources_.assign(*fever, "R-D01", t - 290 * 60).empty()) {
        resources_.changeStatus(*fever, Status::InProgress, t - 280 * 60);
        resources_.changeStatus(*fever, Status::Resolved, t - 240 * 60);
    }
    emergencies_.refresh(t);
    save();
    console_.ok("Added " + std::to_string(added) + " fictional demo records (marked * in tables).");
}

}
