#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "config.h"
#include "executor.h"
#include "mapping.h"
#include "navigation.h"

// The Python server sends one JSON command per line, for example:
// {"cmd": "start_mapping"}

void printJsonStatus(const std::string &status, const std::string &msg = "") {
  std::cout << "{\"type\":\"status\", \"status\": \"" << status
            << "\", \"msg\": \"" << msg << "\"}" << std::endl;
}

int main() {
  printJsonStatus("IDLE", "Robot Core Ready");

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.find("start_mapping") != std::string::npos) {
      printJsonStatus("BUSY", "Starting Mapping...");
      if (runMapping()) {
        printJsonStatus("IDLE", "Mapping Complete");
      } else {
        printJsonStatus("ERROR", "Mapping Failed");
      }
    } else if (line.find("start_navigation") != std::string::npos) {
      printJsonStatus("BUSY", "Starting Navigation...");
      if (runNavigation()) {
        printJsonStatus("IDLE", "Navigation Planning Complete");
      } else {
        printJsonStatus("ERROR", "Navigation Planning Failed");
      }
    } else if (line.find("start_execution") != std::string::npos) {
      printJsonStatus("BUSY", "Starting Execution...");
      if (runExecuteNavigation()) {
        printJsonStatus("IDLE", "Execution Complete");
      } else {
        printJsonStatus("ERROR", "Execution Failed");
      }
      printJsonStatus("ERROR", "Execution Failed");
    }
  }
  else if (line.find("stop") != std::string::npos) {
    printJsonStatus("STOPPED", "Stopping...");
    // For now stop just exits the loop.
    break;
  }
}

return 0;
}
