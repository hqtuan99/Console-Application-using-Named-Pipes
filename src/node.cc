#include "node.h"

#include <iostream>
#include <unordered_set>
#include <vector>

Node::Node(IConnectionMethodFactory &factory)
    : factory_(factory),
      connection_server_(factory.NewServer(*factory.GenerateAddress())),
      connection_client_(factory.NewClient()) {
  LOG(kINFO) << "Creating node...";
  std::unique_ptr<IConnection> connection =
      connection_client_->Connect(*factory.ControllerAddress(), 1000);
  if (!connection) {
    LOG(kDEBUG) << "Could not connect to controller!";
    exit(1);
  }
  Message m;
  m.client_role = role_;
  m.type = MessageType::kNEW_CLIENT;
  connection_server_->address_str().copy(m.addresses[0], kMaxAddressLength);
  if (!connection->Write(m)) {
    LOG(kDEBUG) << "Could not write message to the controller!";
    exit(1);
  }
}

void Node::Run() {
  LOG(kINFO) << "Running node...";
  while (true) {
    switch (role_) {
    case ClientRole::kCLIENT:
      RunAsClient();
      break;
    case ClientRole::kSERVER:
      RunAsServer();
      SendTime();
      break;
    default:
      LOG(kDEBUG) << "Node has incorrect role!";
      return;
    }
  }
}

void Node::RunAsClient() {
  std::unique_ptr<IConnection> connection =
      connection_server_->WaitForConnection(10000);
  if (!connection) {
    exit(1);
  }
  Sleep(1000);
  auto m = connection->Read();
  connection->Close();
  if (!m.is_succeed) {
    LOG(kDEBUG) << "Client " << connection_server_->address_str()
                << " read failed";
    return;
  }
  switch (m.client_role) {
  case ClientRole::kCONTROLLER: {
    if (m.type != MessageType::kSET_SERVER) {
      LOG(kDEBUG) << "Client " << connection_server_->address_str()
                  << " got incorrect message from controller!";
      return;
    }
    LOG(kINFO) << "Becoming server...";
    for (int i = 0; i < m.addresses_count; ++i) {
      clients_.emplace(m.addresses[i]);
    }
    role_ = ClientRole::kSERVER;
    last_time_sending_ = Clock::now();
    return;
  } break;

  case ClientRole::kSERVER: {
    if (m.type != MessageType::kNEW_TIME) {
      LOG(kDEBUG) << "Protocol error: client "
                  << connection_server_->address_str()
                  << " got incorrect message from server";
      return;
    }
    LOG(kINFO) << "Got new time: "
               << SerializeTimePoint(m.time, "UTC: %Y-%m-%d %H:%M:%S");
  } break;

  default:
    LOG(kDEBUG) << "Client " << connection_server_->address_str()
                << " got message from incorrect node!";
  }
}

void Node::SendTime() {
  if (role_ != ClientRole::kSERVER)
    return;

  using namespace std::chrono_literals;
  auto chrono_now = Clock::now();
  if (chrono_now - last_time_sending_ > 1s) {
    LOG(kINFO) << "Sending time...";
    last_time_sending_ = chrono_now;

    Message m;
    m.client_role = role_;
    m.type = MessageType::kNEW_TIME;
    m.time = Clock::now();

    std::vector<decltype(clients_)::iterator> clients_to_delete;
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
      if (*it == connection_server_->address_str())
        continue;
      std::unique_ptr<IAddress> client_address = factory_.NewAddress(*it);
      LOG(kINFO) << "Attempt to connect to " << client_address->raw();
      std::unique_ptr<IConnection> client_connection =
          connection_client_->Connect(*client_address, 100);
      if (!client_connection) {
        clients_to_delete.push_back(it);
        continue;
      }
      if (!client_connection->Write(m)) {
        clients_to_delete.push_back(it);
      }
      client_connection->Close();
    }

    for (auto it : clients_to_delete)
      clients_.erase(it);

    LOG(kINFO) << "Attempt to connect to the controller";
    std::unique_ptr<IConnection> controller_connection =
        connection_client_->Connect(*factory_.ControllerAddress(), 100);
    if (!controller_connection) {
      if (++attempts_to_connect_controller >
          kMaxAttemptsToConnectToController) {
        exit(1);
      } else {
        std::cout << "Could not connect to the controller. Attempt "
                  << attempts_to_connect_controller << '\\'
                  << kMaxAttemptsToConnectToController;
        return;
      }
    }
    if (!controller_connection->Write(m)) {
      if (++attempts_to_connect_controller > 6) {
        exit(1);
      } else {
        std::cout << "Could not write to the controller. Attempt "
                  << attempts_to_connect_controller << '\\'
                  << kMaxAttemptsToConnectToController;
        return;
      }
    }
    attempts_to_connect_controller = 0;
  }
}

void Node::RunAsServer() {
  std::unique_ptr<IConnection> connection =
      connection_server_->WaitForConnection(1000);
  if (!connection) {
    return;
  }
  Message m = connection->Read();
  if (!m.is_succeed) {
    LOG(kDEBUG) << "Server " << connection_server_->address_str()
                << " read failed!";
    return;
  }
  switch (m.client_role) {
  case ClientRole::kCONTROLLER: {
    if (m.type != MessageType::kNEW_CLIENT) {
      LOG(kDEBUG) << "Server " << connection_server_->address_str()
                  << " got incorrect message from controller!";
      return;
    }
    clients_.emplace(m.addresses[0]);
    return;
  } break;

  default:
    LOG(kDEBUG) << "Protocol error: server "
                << connection_server_->address_str()
                << " got message from incorrect node!";
    return;
  }
}
