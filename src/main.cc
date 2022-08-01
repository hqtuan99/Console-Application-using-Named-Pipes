
#include <iostream>
#include <random>

#include "common.h"
#include "controller.h"
#include "node.h"
#include "pipe.h"

namespace {
void run_controller_in_separate_process() {
  LOG(kINFO) << "Starting controller in separate thread...";
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
  LOG(kINFO) << "Testing controller for existence...";
  std::unique_ptr<IClient> client = factory.NewClient();
  // Connection implemented taking into account RAII principle and factory
  // pattern
  std::unique_ptr<IConnection> connection =
      client->Connect(*factory.ControllerAddress(), 500);
  if (!connection) {
    LOG(kINFO) << "Failed!";
    return false;
  }
  Message m;
  m.type = MessageType::kTEST_CONTROLLER;
  if (!connection->Write(m)) {
    LOG(kINFO) << "Failed!";
    return false;
  }
  return true;
}
} // namespace

int main(int argc, const char **argv) {

  LOG(kINFO) << "Got " << argc << " arguments:";
  for (int i = 0; i < argc; ++i) {
    LOG(kINFO) << '\t' << argv[i];
  }
  LOG(kINFO) << '\n';
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> uni(1, 9999);
  uni(rng);

  // Abstract factory was used. To make program use another ipc method it's
  // needed to implement new group of classes and pass new factory to the
  // constructors
  PipeFactory factory;

  for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], "-C") == 0) {
      LOG(kINFO) << "Attempt to run controller...";
      Controller controller(factory);
      controller.Run();
      LOG(kINFO) << "END";
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
  LOG(kINFO) << "Attempt to run node...";
  Node node(factory);
  node.Run();

  return 0;
}
