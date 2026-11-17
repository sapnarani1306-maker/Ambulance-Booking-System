/*
 * =====================================================================
 *  AMBULANCE BOOKING SYSTEM  (Console Application in C++)
 * =====================================================================
 *  Features
 *   - Patients : book an ambulance, check booking status, cancel booking
 *   - Admin    : add / remove ambulances, view fleet, view all bookings,
 *                mark trips as completed  (password protected)
 *   - Fare calculation based on ambulance type and distance
 *   - Data is saved to text files (ambulances.txt, bookings.txt) so it
 *     is NOT lost when the program closes
 *   - Input validation everywhere (no crashes on wrong input)
 *
 *  Compile :  g++ -std=c++11 main.cpp -o ambulance
 *  Run     :  ./ambulance        (Windows: ambulance.exe)
 *  Admin password : admin123
 * =====================================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <algorithm>

using namespace std;

// ----------------------------- Constants -----------------------------
const string AMB_FILE   = "ambulances.txt";
const string BOOK_FILE  = "bookings.txt";
const string ADMIN_PASS = "admin123";

struct TypeInfo {
    string name;
    double baseFare;   // fixed charge in Rs.
    double perKm;      // charge per km in Rs.
};

const TypeInfo TYPES[4] = {
    {"Basic Life Support", 500.0, 20.0},
    {"ICU Ambulance",      1500.0, 40.0},
    {"Cardiac Ambulance",  1200.0, 35.0},
    {"Neonatal Ambulance", 1800.0, 45.0}
};

// ----------------------------- Data models ---------------------------
struct Ambulance {
    int    id;
    string vehicleNo;
    string driverName;
    string driverPhone;
    int    type;          // index into TYPES
    string location;      // current base / area
    bool   available;
};

struct Booking {
    int    id;
    string patientName;
    int    age;
    string phone;
    string pickup;
    string destination;
    int    ambulanceId;
    double distanceKm;
    double fare;
    string status;        // Booked / Completed / Cancelled
    string timeStamp;
};

// ----------------------------- Helpers -------------------------------
string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

vector<string> split(const string &s, char delim) {
    vector<string> parts;
    string item;
    stringstream ss(s);
    while (getline(ss, item, delim)) parts.push_back(item);
    return parts;
}

string currentTime() {
    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M", localtime(&now));
    return string(buf);
}

// Reads a non-empty line (the '|' character is replaced because it is
// used as the file separator).
string readLine(const string &prompt) {
    string s;
    while (true) {
        cout << prompt;
        if (!getline(cin, s)) exit(0);          // end of input
        s = trim(s);
        replace(s.begin(), s.end(), '|', ' ');
        if (!s.empty()) return s;
        cout << "  Input cannot be empty. Try again.\n";
    }
}

int readInt(const string &prompt, int lo, int hi) {
    string s;
    while (true) {
        cout << prompt;
        if (!getline(cin, s)) exit(0);
        try {
            size_t pos;
            int v = stoi(trim(s), &pos);
            if (pos == trim(s).size() && v >= lo && v <= hi) return v;
        } catch (...) {}
        cout << "  Please enter a number between " << lo << " and " << hi << ".\n";
    }
}

double readDouble(const string &prompt, double lo, double hi) {
    string s;
    while (true) {
        cout << prompt;
        if (!getline(cin, s)) exit(0);
        try {
            size_t pos;
            double v = stod(trim(s), &pos);
            if (pos == trim(s).size() && v >= lo && v <= hi) return v;
        } catch (...) {}
        cout << "  Please enter a value between " << lo << " and " << hi << ".\n";
    }
}

bool validPhone(const string &p) {
    if (p.size() != 10) return false;
    return all_of(p.begin(), p.end(), [](char c) { return c >= '0' && c <= '9'; });
}

string readPhone(const string &prompt) {
    while (true) {
        string p = readLine(prompt);
        if (validPhone(p)) return p;
        cout << "  Phone number must be exactly 10 digits.\n";
    }
}

void pause() {
    cout << "\nPress Enter to continue...";
    string tmp;
    getline(cin, tmp);
}

void line(char c = '-', int n = 78) { cout << string(n, c) << "\n"; }

void header(const string &title) {
    cout << "\n";
    line('=');
    cout << "  " << title << "\n";
    line('=');
}

// ----------------------------- Main system class ---------------------
class AmbulanceSystem {
private:
    vector<Ambulance> ambulances;
    vector<Booking>   bookings;
    int nextAmbId  = 1;
    int nextBookId = 1001;

    // ---------- file handling ----------
    void saveAmbulances() {
        ofstream out(AMB_FILE);
        for (const auto &a : ambulances)
            out << a.id << '|' << a.vehicleNo << '|' << a.driverName << '|'
                << a.driverPhone << '|' << a.type << '|' << a.location << '|'
                << (a.available ? 1 : 0) << '\n';
    }

    void saveBookings() {
        ofstream out(BOOK_FILE);
        for (const auto &b : bookings)
            out << b.id << '|' << b.patientName << '|' << b.age << '|' << b.phone << '|'
                << b.pickup << '|' << b.destination << '|' << b.ambulanceId << '|'
                << b.distanceKm << '|' << b.fare << '|' << b.status << '|'
                << b.timeStamp << '\n';
    }

    void loadAmbulances() {
        ifstream in(AMB_FILE);
        string ln;
        while (getline(in, ln)) {
            auto p = split(ln, '|');
            if (p.size() < 7) continue;
            try {
                Ambulance a;
                a.id          = stoi(p[0]);
                a.vehicleNo   = p[1];
                a.driverName  = p[2];
                a.driverPhone = p[3];
                a.type        = stoi(p[4]);
                a.location    = p[5];
                a.available   = (p[6] == "1");
                if (a.type < 0 || a.type > 3) continue;
                ambulances.push_back(a);
                nextAmbId = max(nextAmbId, a.id + 1);
            } catch (...) {}
        }
    }

    void loadBookings() {
        ifstream in(BOOK_FILE);
        string ln;
        while (getline(in, ln)) {
            auto p = split(ln, '|');
            if (p.size() < 11) continue;
            try {
                Booking b;
                b.id          = stoi(p[0]);
                b.patientName = p[1];
                b.age         = stoi(p[2]);
                b.phone       = p[3];
                b.pickup      = p[4];
                b.destination = p[5];
                b.ambulanceId = stoi(p[6]);
                b.distanceKm  = stod(p[7]);
                b.fare        = stod(p[8]);
                b.status      = p[9];
                b.timeStamp   = p[10];
                bookings.push_back(b);
                nextBookId = max(nextBookId, b.id + 1);
            } catch (...) {}
        }
    }

    // ---------- lookup helpers ----------
    Ambulance *findAmbulance(int id) {
        for (auto &a : ambulances) if (a.id == id) return &a;
        return nullptr;
    }

    Booking *findBooking(int id) {
        for (auto &b : bookings) if (b.id == id) return &b;
        return nullptr;
    }

    void addDefaultFleet() {
        ambulances.push_back({nextAmbId++, "PB10-AB-1234", "Gurpreet Singh", "9876543210", 0, "Ludhiana", true});
        ambulances.push_back({nextAmbId++, "PB10-CD-5678", "Rajesh Kumar",   "9812345678", 1, "Ludhiana", true});
        ambulances.push_back({nextAmbId++, "PB65-EF-9012", "Amandeep Kaur",  "9988776655", 2, "Khanna",   true});
        ambulances.push_back({nextAmbId++, "PB10-GH-3456", "Harjit Singh",   "9123456780", 3, "Ludhiana", true});
        saveAmbulances();
    }

    void printAmbulanceRow(const Ambulance &a) {
        cout << left << setw(5)  << a.id
             << setw(15) << a.vehicleNo
             << setw(18) << a.driverName
             << setw(13) << a.driverPhone
             << setw(20) << TYPES[a.type].name
             << setw(12) << a.location
             << (a.available ? "Available" : "On Duty") << "\n";
    }

    void printAmbulanceHeader() {
        line();
        cout << left << setw(5)  << "ID"
             << setw(15) << "Vehicle No"
             << setw(18) << "Driver"
             << setw(13) << "Phone"
             << setw(20) << "Type"
             << setw(12) << "Location"
             << "Status\n";
        line();
    }

    void printBooking(const Booking &b) {
        cout << "\n";
        line('-', 50);
        cout << " Booking ID   : " << b.id << "\n"
             << " Patient      : " << b.patientName << " (Age " << b.age << ")\n"
             << " Contact      : " << b.phone << "\n"
             << " Pickup       : " << b.pickup << "\n"
             << " Destination  : " << b.destination << "\n";
        Ambulance *a = findAmbulance(b.ambulanceId);
        if (a) {
            cout << " Ambulance    : " << a->vehicleNo << " (" << TYPES[a->type].name << ")\n"
                 << " Driver       : " << a->driverName << " - " << a->driverPhone << "\n";
        } else {
            cout << " Ambulance    : (record removed) ID " << b.ambulanceId << "\n";
        }
        cout << fixed << setprecision(2)
             << " Distance     : " << b.distanceKm << " km\n"
             << " Fare         : Rs. " << b.fare << "\n"
             << " Status       : " << b.status << "\n"
             << " Booked on    : " << b.timeStamp << "\n";
        line('-', 50);
    }

public:
    AmbulanceSystem() {
        loadAmbulances();
        loadBookings();
        if (ambulances.empty()) addDefaultFleet();   // first run
    }

    // ================= PATIENT FUNCTIONS =================
    void bookAmbulance() {
        header("BOOK AN AMBULANCE");

        cout << "\nSelect ambulance type:\n";
        for (int i = 0; i < 4; i++)
            cout << "  " << i + 1 << ". " << left << setw(20) << TYPES[i].name
                 << " Base Rs." << TYPES[i].baseFare
                 << " + Rs." << TYPES[i].perKm << "/km\n";
        int type = readInt("Choice (1-4): ", 1, 4) - 1;

        // list available ambulances of that type
        vector<Ambulance *> options;
        for (auto &a : ambulances)
            if (a.type == type && a.available) options.push_back(&a);

        if (options.empty()) {
            cout << "\n  Sorry, no " << TYPES[type].name
                 << " is available right now. Please try another type or call again later.\n";
            return;
        }

        cout << "\nAvailable ambulances:\n";
        printAmbulanceHeader();
        for (auto *a : options) printAmbulanceRow(*a);
        line();

        // patient details
        cout << "\nEnter patient details\n";
        string name = readLine("Patient name        : ");
        int    age  = readInt("Patient age         : ", 0, 120);
        string phone = readPhone("Contact number (10) : ");
        string pickup = readLine("Pickup address      : ");
        string dest   = readLine("Destination hospital: ");
        double dist   = readDouble("Approx distance (km): ", 0.1, 1000);

        // choose ambulance
        int ambId;
        while (true) {
            ambId = readInt("Enter Ambulance ID to book: ", 1, 1000000);
            Ambulance *a = findAmbulance(ambId);
            if (a && a->available && a->type == type) break;
            cout << "  Invalid choice. Pick an ID from the available list above.\n";
        }

        double fare = TYPES[type].baseFare + TYPES[type].perKm * dist;

        cout << fixed << setprecision(2);
        cout << "\nEstimated fare: Rs. " << fare << "\n";
        int ok = readInt("Confirm booking? (1 = Yes, 0 = No): ", 0, 1);
        if (!ok) { cout << "  Booking discarded.\n"; return; }

        Booking b;
        b.id          = nextBookId++;
        b.patientName = name;
        b.age         = age;
        b.phone       = phone;
        b.pickup      = pickup;
        b.destination = dest;
        b.ambulanceId = ambId;
        b.distanceKm  = dist;
        b.fare        = fare;
        b.status      = "Booked";
        b.timeStamp   = currentTime();
        bookings.push_back(b);

        findAmbulance(ambId)->available = false;
        saveBookings();
        saveAmbulances();

        cout << "\n  *** BOOKING CONFIRMED ***";
        printBooking(b);
        cout << " Please note your Booking ID: " << b.id << "\n";
    }

    void checkStatus() {
        header("CHECK BOOKING STATUS");
        int id = readInt("Enter Booking ID: ", 1, 100000000);
        Booking *b = findBooking(id);
        if (!b) cout << "  No booking found with ID " << id << ".\n";
        else    printBooking(*b);
    }

    void cancelBooking() {
        header("CANCEL BOOKING");
        int id = readInt("Enter Booking ID: ", 1, 100000000);
        Booking *b = findBooking(id);
        if (!b) { cout << "  No booking found with ID " << id << ".\n"; return; }
        if (b->status != "Booked") {
            cout << "  This booking is already " << b->status << " and cannot be cancelled.\n";
            return;
        }
        string phone = readPhone("Verify - enter the contact number used for booking: ");
        if (phone != b->phone) { cout << "  Phone number does not match. Cancellation denied.\n"; return; }

        b->status = "Cancelled";
        Ambulance *a = findAmbulance(b->ambulanceId);
        if (a) a->available = true;
        saveBookings();
        saveAmbulances();
        cout << "  Booking " << id << " cancelled successfully.\n";
    }

    // ================= ADMIN FUNCTIONS =================
    void addAmbulance() {
        header("ADD NEW AMBULANCE");
        Ambulance a;
        a.id = nextAmbId++;
        a.vehicleNo   = readLine("Vehicle number : ");
        a.driverName  = readLine("Driver name    : ");
        a.driverPhone = readPhone("Driver phone (10 digits): ");
        cout << "Types: 1.Basic  2.ICU  3.Cardiac  4.Neonatal\n";
        a.type      = readInt("Type (1-4)     : ", 1, 4) - 1;
        a.location  = readLine("Base location  : ");
        a.available = true;
        ambulances.push_back(a);
        saveAmbulances();
        cout << "  Ambulance added with ID " << a.id << ".\n";
    }

    void removeAmbulance() {
        header("REMOVE AMBULANCE");
        viewAmbulances(false);
        int id = readInt("Enter Ambulance ID to remove: ", 1, 1000000);
        Ambulance *a = findAmbulance(id);
        if (!a) { cout << "  No such ambulance.\n"; return; }
        if (!a->available) { cout << "  Ambulance is currently on duty. Complete/cancel its trip first.\n"; return; }
        ambulances.erase(remove_if(ambulances.begin(), ambulances.end(),
                         [id](const Ambulance &x) { return x.id == id; }), ambulances.end());
        saveAmbulances();
        cout << "  Ambulance removed.\n";
    }

    void viewAmbulances(bool showTitle = true) {
        if (showTitle) header("AMBULANCE FLEET");
        if (ambulances.empty()) { cout << "  No ambulances registered.\n"; return; }
        printAmbulanceHeader();
        for (const auto &a : ambulances) printAmbulanceRow(a);
        line();
    }

    void viewBookings() {
        header("ALL BOOKINGS");
        if (bookings.empty()) { cout << "  No bookings yet.\n"; return; }
        line();
        cout << left << setw(8)  << "ID"
             << setw(20) << "Patient"
             << setw(10) << "Amb ID"
             << setw(12) << "Fare(Rs)"
             << setw(12) << "Status"
             << "Booked on\n";
        line();
        for (const auto &b : bookings)
            cout << left << setw(8)  << b.id
                 << setw(20) << b.patientName
                 << setw(10) << b.ambulanceId
                 << setw(12) << fixed << setprecision(2) << b.fare
                 << setw(12) << b.status
                 << b.timeStamp << "\n";
        line();
    }

    void completeTrip() {
        header("COMPLETE TRIP");
        int id = readInt("Enter Booking ID: ", 1, 100000000);
        Booking *b = findBooking(id);
        if (!b) { cout << "  No booking found.\n"; return; }
        if (b->status != "Booked") { cout << "  Booking is already " << b->status << ".\n"; return; }
        b->status = "Completed";
        Ambulance *a = findAmbulance(b->ambulanceId);
        if (a) {
            a->available = true;
            a->location  = b->destination;   // ambulance is now at the hospital
        }
        saveBookings();
        saveAmbulances();
        cout << "  Trip marked completed. Ambulance is available again.\n";
    }

    void revenueReport() {
        header("REVENUE REPORT");
        double total = 0;
        int done = 0, active = 0, cancelled = 0;
        for (const auto &b : bookings) {
            if (b.status == "Completed") { total += b.fare; done++; }
            else if (b.status == "Booked") active++;
            else cancelled++;
        }
        cout << fixed << setprecision(2)
             << "  Completed trips : " << done << "\n"
             << "  Active bookings : " << active << "\n"
             << "  Cancelled       : " << cancelled << "\n"
             << "  Total revenue   : Rs. " << total << "\n";
    }

    // ================= MENUS =================
    void adminMenu() {
        string pass = readLine("\nEnter admin password: ");
        if (pass != ADMIN_PASS) { cout << "  Wrong password!\n"; return; }

        while (true) {
            header("ADMIN PANEL");
            cout << "  1. Add ambulance\n"
                 << "  2. Remove ambulance\n"
                 << "  3. View all ambulances\n"
                 << "  4. View all bookings\n"
                 << "  5. Mark trip as completed\n"
                 << "  6. Revenue report\n"
                 << "  0. Back to main menu\n";
            int ch = readInt("Choice: ", 0, 6);
            switch (ch) {
                case 1: addAmbulance();   break;
                case 2: removeAmbulance();break;
                case 3: viewAmbulances(); break;
                case 4: viewBookings();   break;
                case 5: completeTrip();   break;
                case 6: revenueReport();  break;
                case 0: return;
            }
            pause();
        }
    }

    void mainMenu() {
        while (true) {
            header("AMBULANCE BOOKING SYSTEM");
            cout << "  1. Book an ambulance\n"
                 << "  2. Check booking status\n"
                 << "  3. Cancel a booking\n"
                 << "  4. View available ambulances\n"
                 << "  5. Admin login\n"
                 << "  0. Exit\n";
            int ch = readInt("Choice: ", 0, 5);
            switch (ch) {
                case 1: bookAmbulance(); break;
                case 2: checkStatus();   break;
                case 3: cancelBooking(); break;
                case 4: viewAmbulances();break;
                case 5: adminMenu();     continue;   // admin menu handles its own pause
                case 0:
                    cout << "\nThank you for using the Ambulance Booking System. Stay safe!\n";
                    return;
            }
            pause();
        }
    }
};

// ----------------------------- Entry point ---------------------------
int main() {
    AmbulanceSystem system;
    system.mainMenu();
    return 0;
}
