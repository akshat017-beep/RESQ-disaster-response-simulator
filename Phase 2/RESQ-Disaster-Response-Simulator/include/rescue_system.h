#ifndef NITIN_CLASSES_H
#define NITIN_CLASSES_H

#include <string>
#include <vector>
#include <ctime>

class Location {
private:
    int id_;
    std::string name_;
public:
    Location(int id = 0, const std::string& name = "");
    int id() const;
    const std::string& name() const;
    bool setName(const std::string& name);
};

class Disaster {
private:
    int id_;
    std::string type_;
    std::string area_;
    int severity_;
    int affected_;
public:
    Disaster(int id = 0, const std::string& type = "", const std::string& area = "", int severity = 1, int affected = 0);
    int id() const;
    const std::string& type() const;
    const std::string& area() const;
    int severity() const;
    int affected() const;
    bool updateSeverity(int severity);
    bool updateAffected(int affected);
};

class Victim {
private:
    int id_;
    std::string name_;
    std::string location_;
    std::string description_;
    int affected_;
    int severity_;
    int status_;
public:
    Victim(int id, const std::string& name, const std::string& location, const std::string& description, int affected, int severity);
    int id() const;
    const std::string& name() const;
    const std::string& location() const;
    const std::string& description() const;
    int affected() const;
    int severity() const;
    int status() const;
    int priorityScore() const;
    bool setStatus(int status);
    void print() const;
};

class RescueTeam {
protected:
    int id_;
    std::string name_;
    std::string location_;
    int capacity_;
    bool available_;
public:
    RescueTeam(int id, const std::string& name, const std::string& location, int capacity);
    virtual ~RescueTeam();
    int id() const;
    const std::string& name() const;
    const std::string& location() const;
    int capacity() const;
    bool available() const;
    virtual std::string dispatchType() const;
    virtual bool dispatch(int people);
    virtual void print() const;
    bool moveTo(const std::string& location);
    bool release();
};

class Ambulance : public RescueTeam {
private:
    int patientCapacity_;
public:
    Ambulance(int id, const std::string& name, const std::string& location, int patientCapacity);
    std::string dispatchType() const override;
    bool dispatch(int people) override;
    void print() const override;
};

class RescueCrew : public RescueTeam {
private:
    int crewMembers_;
public:
    RescueCrew(int id, const std::string& name, const std::string& location, int crewMembers);
    std::string dispatchType() const override;
    bool dispatch(int people) override;
    void print() const override;
};

class Hospital {
private:
    int id_;
    std::string name_;
    std::string location_;
    int beds_;
    int occupied_;
public:
    Hospital(int id, const std::string& name, const std::string& location, int beds);
    int id() const;
    const std::string& name() const;
    const std::string& location() const;
    int availableBeds() const;
    bool admit(int people);
    bool discharge(int people);
    void print() const;
};

class RescueOperation {
private:
    int id_;
    int victimId_;
    int teamId_;
    std::string route_;
    int routeCost_;
    std::string status_;
    std::time_t createdAt_;
public:
    RescueOperation(int id, int victimId, int teamId, const std::string& route, int routeCost);
    int id() const;
    int victimId() const;
    int teamId() const;
    const std::string& route() const;
    int routeCost() const;
    const std::string& status() const;
    bool start();
    bool complete();
    bool cancel();
    void print() const;
};

class ResqScenarioManager {
private:
    std::vector<Victim> victims_;
    std::vector<RescueTeam*> teams_;
    std::vector<Hospital> hospitals_;
    std::vector<Disaster> disasters_;
    std::vector<Location> locations_;
    std::vector<RescueOperation> operations_;
    int nextVictimId_;
    int nextTeamId_;
    int nextHospitalId_;
    int nextOperationId_;
public:
    ResqScenarioManager();
    ~ResqScenarioManager();
    ResqScenarioManager(const ResqScenarioManager&) = delete;
    ResqScenarioManager& operator=(const ResqScenarioManager&) = delete;
    int addVictim(const std::string& name, const std::string& location, const std::string& description, int affected, int severity);
    int addAmbulance(const std::string& name, const std::string& location, int capacity);
    int addRescueCrew(const std::string& name, const std::string& location, int members);
    int addHospital(const std::string& name, const std::string& location, int beds);
    int addDisaster(const std::string& type, const std::string& area, int severity, int affected);
    int addLocation(const std::string& name);
    bool addRoad(int fromIndex, int toIndex, int weight);
    bool dispatchVictim(int victimId, int teamId, const std::string& route, int routeCost);
    bool startOperation(int operationId);
    bool completeOperation(int operationId);
    bool cancelOperation(int operationId);
    bool admitVictim(int victimId, int hospitalId, int people);
    void listVictims() const;
    void listTeams() const;
    void listHospitals() const;
    void listDisasters() const;
    void listLocations() const;
    void listOperations() const;
    void showSummary() const;
    void runScenarioTests() const;
    Victim* findVictim(int id);
    RescueTeam* findTeam(int id);
    Hospital* findHospital(int id);
    RescueOperation* findOperation(int id);
};

#endif
