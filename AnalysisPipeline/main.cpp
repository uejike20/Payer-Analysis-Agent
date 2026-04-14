#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include "Payers.h"

using namespace std;
namespace fs = std::filesystem;

// Runtime artifacts are written outside payer_sources so source inputs stay clean.
static const char* OUTPUT_ROOT = "outputs";
static const char* OUTPUT_PATH = "outputs/extracted_route_data.json";
static const char* STATE_PATH = "outputs/agent_state.txt";
static const char* PAYER_SOURCES_ROOT = "payer_sources";

struct AgentState {
    int consecutive_failures = 0;
    long long last_success_epoch = 0;
};

string makeDisplayName(const string& payerKey) {
    // Convert folder keys like "blue_cross_blue_shield" to "Blue Cross Blue Shield".
    string displayName;
    bool capitalizeNext = true;

    for (size_t i = 0; i < payerKey.size(); ++i) {
        const char ch = payerKey[i];
        if (ch == '_' || ch == '-') {
            displayName += ' ';
            capitalizeNext = true;
            continue;
        }

        if (capitalizeNext) {
            displayName += static_cast<char>(toupper(static_cast<unsigned char>(ch)));
            capitalizeNext = false;
        } else {
            displayName += static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        }
    }

    return displayName;
}

bool hasAnySourceFile(const fs::path& payerDir) {
    // A payer folder is considered valid if it has at least one known source type.
    static const vector<string> requiredSourceFiles = {
        "provider_manual.txt",
        "phone_transcript.txt",
        "web_page.txt",
        "denial_letter.txt"
    };

    for (size_t i = 0; i < requiredSourceFiles.size(); ++i) {
        if (fs::exists(payerDir / requiredSourceFiles[i])) {
            return true;
        }
    }
    return false;
}

vector<Payers> discoverPayers() {
    // Discover payer folders dynamically so adding/removing payers is data-driven.
    vector<Payers> payers;
    if (!fs::exists(PAYER_SOURCES_ROOT) || !fs::is_directory(PAYER_SOURCES_ROOT)) {
        cerr << "Payer source root directory not found: " << PAYER_SOURCES_ROOT << "\n";
        return payers;
    }

    vector<string> payerKeys;
    for (const auto& entry : fs::directory_iterator(PAYER_SOURCES_ROOT)) {
        if (!entry.is_directory()) {
            continue;
        }

        const string payerKey = entry.path().filename().string();
        if (payerKey.empty() || payerKey[0] == '.') {
            continue;
        }

        if (hasAnySourceFile(entry.path())) {
            payerKeys.push_back(payerKey);
        }
    }

    sort(payerKeys.begin(), payerKeys.end());
    for (size_t i = 0; i < payerKeys.size(); ++i) {
        payers.push_back(Payers(payerKeys[i], makeDisplayName(payerKeys[i])));
    }

    return payers;
}

void saveState(const AgentState& state) {
    // Persist failure counters across restarts for stable retry behavior.
    ofstream stateFile(STATE_PATH);
    if (!stateFile.is_open()) {
        cerr << "Warning: could not write state file: " << STATE_PATH << "\n";
        return;
    }
    stateFile << "consecutive_failures=" << state.consecutive_failures << "\n";
    stateFile << "last_success_epoch=" << state.last_success_epoch << "\n";
}

AgentState loadState() {
    // Missing state file is normal on first run.
    AgentState state;
    ifstream stateFile(STATE_PATH);
    if (!stateFile.is_open()) {
        return state;
    }

    string line;
    while (getline(stateFile, line)) {
        const string failuresKey = "consecutive_failures=";
        const string successKey = "last_success_epoch=";

        if (line.rfind(failuresKey, 0) == 0) {
            state.consecutive_failures = stoi(line.substr(failuresKey.size()));
        } else if (line.rfind(successKey, 0) == 0) {
            state.last_success_epoch = stoll(line.substr(successKey.size()));
        }
    }
    return state;
}

bool runOnce() {
    try {
        // Ensure output directory exists before writing JSON/state.
        fs::create_directories(OUTPUT_ROOT);

        vector<Payers> payers = discoverPayers();
        if (payers.empty()) {
            cerr << "No payer folders with source files found under " << PAYER_SOURCES_ROOT << ".\n";
            return false;
        }

        // Build payer objects dynamically from directories in payer_sources/.
        for (size_t i = 0; i < payers.size(); ++i) {
            cout << "--- " << payers[i].getPayerDisplayName() << " (" << payers[i].getPayerKey() << ") ---\n";
            payers[i].readAllSources();
        }

        ofstream outFile(OUTPUT_PATH);
        if (!outFile.is_open()) {
            cerr << "Error opening output file: " << OUTPUT_PATH << "\n";
            return false;
        }

        outFile << "{\n";
        for (size_t i = 0; i < payers.size(); ++i) {
            outFile << payers[i].toJsonEntry();
            if (i + 1 < payers.size()) {
                outFile << ",\n";
            } else {
                outFile << '\n';
            }
        }
        outFile << "}\n";
        outFile.close();

        cout << "Wrote " << OUTPUT_PATH << "\n";
        return true;
    } catch (const exception& ex) {
        cerr << "Run failed with exception: " << ex.what() << "\n";
    } catch (...) {
        cerr << "Run failed with unknown exception.\n";
    }
    return false;
}

int main() {
    AgentState state = loadState();
    const char* runOnceEnv = std::getenv("AUTHPIPELINE_RUN_ONCE");
    const bool runOnceMode = runOnceEnv != nullptr &&
                             (string(runOnceEnv) == "1" || string(runOnceEnv) == "true" || string(runOnceEnv) == "TRUE");

    const auto normalInterval = chrono::minutes(5);
    const auto failureBackoff = chrono::seconds(30);
    const int maxConsecutiveFailures = 5;

    cout << "AuthPipeline agent started. Interval=5 minutes.\n";
    while (true) {
        // The agent runs continuously; orchestrators can restart it on fatal errors.
        bool success = runOnce();
        if (success) {
            state.consecutive_failures = 0;
            state.last_success_epoch = static_cast<long long>(time(nullptr));
            saveState(state);
            if (runOnceMode) {
                return 0;
            }
            this_thread::sleep_for(normalInterval);
        } else {
            ++state.consecutive_failures;
            saveState(state);
            if (runOnceMode) {
                return 1;
            }

            if (state.consecutive_failures >= maxConsecutiveFailures) {
                cerr << "Agent exiting after " << state.consecutive_failures
                     << " consecutive failures.\n";
                return 1;
            }

            cerr << "Retrying after failure backoff (" << state.consecutive_failures
                 << "/" << maxConsecutiveFailures << ").\n";
            this_thread::sleep_for(failureBackoff);
        }
    }
}
