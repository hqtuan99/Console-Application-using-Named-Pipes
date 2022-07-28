#include <windows.h>
#include <winuser.h>

#include <iostream>
#include <ostream>

#include "common.h"
#include "controller.h"
#include "node.h"
#include "pipe.h"

void run_controller_in_separate_process() {
  std::cout << "Starting controller in separate thread..." << std::endl;
  STARTUPINFO si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  char current_file_path[1024];
  GetModuleFileNameA(nullptr, current_file_path, 1024);
  char command_line[] = "-C";
  if (!CreateProcessA(current_file_path, command_line, nullptr, nullptr, false,
                      CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
    WriteLastErrorMessage("Main::CreateProcess");
  }
}

bool test_controller_pipe(IConnectionMethodFactory &factory) {
  std::cout << "Testing controller for existence..." << std::endl;
  auto client = factory.NewClient();
  // Connection implemented taking into account RAII principle and factory
  // pattern
  auto connection = client->Connect(*factory.ControllerAddress(), 500);
  if (!connection) {
    std::cout << "Failed\n";
    return false;
  }
  Message m;
  m.type = MessageType::TEST_CONTROLLER;
  if (!connection->Write(m)) {
    std::cout << "Failed\n";
    return false;
  }
  return true;
}

int main(int argc, const char **argv) {
  std::cout << "Got " << argc << " arguments:" << std::endl;
  for (int i = 0; i < argc; ++i) {
    std::cout << '\t' << argv[i] << std::endl;
  }
  std::cout << '\n';
  srand(time(nullptr));

  // Abstract factory was used. To make program use another ipc method it's
  // needed to implement new group of classes and pass new factory to the
  // constructors
  PipeFactory factory;

  for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], "-C") == 0) {
      std::cout << "Attempt to run controller..." << std::endl;
      Controller controller(factory);
      controller.Run();
      std::cout << "END" << std::endl;
      char c;
      std::cin >> c;
      return 0;
    }
  }

  if (!test_controller_pipe(factory)) {
    run_controller_in_separate_process();
    // sleep for giving the controller time to initialize
    Sleep(200);
  }
  std::cout << "Attempt to run node..." << std::endl;
  Node node(factory);
  node.Run();

  return 0;
}
