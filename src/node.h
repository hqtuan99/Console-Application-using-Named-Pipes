#ifndef NODE_H_
#define NODE_H_

#include <unordered_set>

#include "common.h"
#include "i_connection_method.h"

class Node {
public:
  Node(IConnectionMethodFactory &factory);
  Node(Node &&) = delete;
  Node(const Node &) = delete;
  Node &operator=(Node &&) = delete;
  Node &operator=(const Node &) = delete;

  void Run();

private:
  void RunAsClient();
  void RunAsServer();
  void SendTime();
  ClientRole role_ = ClientRole::CLIENT;
  IConnectionMethodFactory &factory_;
  // should be used only by server
  std::unordered_set<std::string> clients_;
  int attempts_to_connect_controller = 0;
  static const int kMaxAttemptsToConnectToController = 6;
  std::unique_ptr<IServer> connection_server_;
  std::unique_ptr<IClient> connection_client_;
  TimePoint last_time_sending_;
};

#endif // NODE_H_