#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>

#include "common.h"
#include "pipe.h"

class Controller {
public:
  Controller(IConnectionMethodFactory &factory);
  Controller(Controller &&) = delete;
  Controller(const Controller &) = delete;
  Controller &operator=(Controller &&) = delete;
  Controller &operator=(const Controller &) = delete;

  void Run();

private:
  static const Clock::duration max_server_response;
  static constexpr ClientRole role = ClientRole::CONTROLLER;
  void ChooseNewServer();
  IConnectionMethodFactory &connection_factory_;
  std::unordered_set<std::string> connected_nodes_addresses_;
  std::unique_ptr<IAddress> server_address_;
  TimePoint last_server_response_{};
  std::unique_ptr<IServer> connection_server_;
  std::unique_ptr<IClient> connection_client_;
};

#endif // CONTROLLER_H_
