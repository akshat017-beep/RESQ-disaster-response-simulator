#ifndef RESQ_APP_H
#define RESQ_APP_H

#include "analytics.h"
#include "console.h"
#include "dispatcher.h"
#include "emergency_manager.h"
#include "file_manager.h"
#include "resource_manager.h"

#include <ctime>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace resq {

constexpr const char* kVersion = "1.0.0";

struct AppOptions {
    std::string dataDirectory = "data";
    int color = -1;
    int unicode = -1;
    bool animations = true;
    bool showHelp = false;
    bool showVersion = false;

    static bool parse(int argc, char* argv[], AppOptions& options, std::string& error);
    static std::string usage();
};

class RESQApplication {
public:
    explicit RESQApplication(const AppOptions& options);
    int run();

private:
    void startup();
    void mainLoop();
    void shutdown();
    bool persist();
    void log(const std::string& activity);
    std::time_t now() const;

    void showDashboard();
    void registerEmergency();
    void viewEmergencies();
    void searchEmergencies();
    void updateEmergency();
    void changeStatusFlow(const std::string& emergencyId);

    void dispatchMenu();
    void processNextEmergency();
    void viewPriorityQueue();
    void manageActiveOperations();
    void manageOperation(const std::string& emergencyId);
    bool assignResourceFlow(const std::string& emergencyId);

    void resourceMenu();
    void addResource();
    void viewResources();
    void searchResources();
    void updateResourceAvailability();
    void assignResourceMenu();
    void releaseResourceMenu();
    void viewAssignmentHistory();

    void reportsMenu();
    void reportOverview();
    void reportBreakdown();
    void reportResources();
    void reportTiming();
    void demoMenu();
    void settingsMenu();
    void showDataStatus();
    void showPriorityHelp();
    void showAbout();
    void showHelp();

    void printEmergencyCard(const Emergency& emergency);
    void printScoreBreakdown(const Emergency& emergency);
    void printEmergencyTable(const std::vector<const Emergency*>& list);
    void printResourceTable(const std::vector<const Resource*>& list);
    void printResourceCard(const Resource& resource);
    std::vector<SuitableResource> printSuitableResources(const Emergency& emergency);
    std::vector<std::string> statusLines(const std::vector<std::pair<std::string, std::pair<std::string, Style>>>& rows,
                                         int width) const;

    Category pickCategory();
    Severity pickSeverity();
    ResourceType pickResourceType();
    std::string readValidated(const std::string& prompt, size_t maxLength,
                              const std::function<std::string(const std::string&)>& validate);
    std::time_t readRegistrationTime();
    Emergency* promptEmergency(const std::string& prompt);
    Resource* promptResource(const std::string& prompt);

    AppOptions options_;
    Console console_;
    EmergencyManager emergencies_;
    ResourceManager resources_;
    FileManager files_;
    Dispatcher dispatcher_;
    AnalyticsManager analytics_;
    LoadReport loadReport_;
    std::vector<std::string> reconcileNotes_;
    std::deque<std::string> recentActivity_;
    std::time_t lastSaved_ = 0;
    int exitCode_ = 0;
};

}

#endif
