#include "node.h"

#include <corecrt.h>

#include <iostream>
#include <memory>
#include <ostream>
#include <unordered_set>
#include <vector>

#include "controller.h"

Node::Node(IConnectionMethodFactory &factory)
    : factory_(factory),
      connection_server_(factory.NewServer(*factory.GenerateAddress())),
      connection_client_(factory.NewClient()) {
  std::cout << "Creating node..." << std::endl;
  std::unique_ptr<IConnection> connection =
      connection_client_->Connect(*factory.ControllerAddress(), 1000);
  if (!connection) {
    std::cerr << "Could not connect to controller!" << std::endl;
    exit(1);
  }
  Message m;
  m.client_role = role_;
  m.type = MessageType::NEW_CLIENT;
  connection_server_->address_str().copy(m.addresses[0], kMaxAddressLength);
  if (!connection->Write(m)) {
    std::cerr << "Could not write message to the controller!" << std::endl;
    exit(1);
  }
}

void Node::Run() {
  std::cout << "Running node..." << std::endl;
  while (true) {
    switch (role_) {
    case ClientRole::CLIENT:
      RunAsClient();
      break;
    case ClientRole::SERVER:
      RunAsServer();
      SendTime();
      break;
    default:
      std::cerr << "Node has incorrect role!" << std::endl;
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
    std::cerr << "Client " << connection_server_->address_str()
              << " read failed";
    return;
  }
  switch (m.client_role) {
  case ClientRole::CONTROLLER: {
    if (m.type != MessageType::SET_SERVER) {
      std::cerr << "Client " << connection_server_->address_str()
                << " got incorrect message from controller!" << std::endl;
      return;
    }
    std::cout << "Becoming server..." << std::endl;
    for (int i = 0; i < m.addresses_count; ++i) {
      clients_.emplace(m.addresses[i]);
    }
    role_ = ClientRole::SERVER;
    last_time_sending_ = Clock::now();
    return;
  } break;

  case ClientRole::SERVER: {
    if (m.type != MessageType::NEW_TIME) {
      std::cerr << "Protocol error: client "
                << connection_server_->address_str()
                << " got incorrect message from server" << std::endl;
      return;
    }
    std::cout << "Got new time: "
              << serializeTimePoint(m.time, "UTC: %Y-%m-%d %H:%M:%S")
              << std::endl;
  } break;

  default:
    std::cerr << "Client " << connection_server_->address_str()
              << " got message from incorrect node!" << std::endl;
  }
}

void Node::SendTime() {
  if (role_ != ClientRole::SERVER)
    return;

  using namespace std::chrono_literals;
  auto chrono_now = Clock::now();
  if (chrono_now - last_time_sending_ > 1s) {
    std::cout << "Sending time..." << std::endl;
    last_time_sending_ = chrono_now;

    Message m;
    m.client_role = role_;
    m.type = MessageType::NEW_TIME;
    m.time = Clock::now();

    std::vector<decltype(clients_)::iterator> clients_to_delete;
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
      if (*it == connection_server_->address_str())
        continue;
      std::unique_ptr<IAddress> client_address = factory_.NewAddress(*it);
      std::cout << "Attempt to connect to " << client_address->raw()
                << std::endl;
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

    std::cout << "Attempt to connect to the controller" << std::endl;
    std::unique_ptr<IConnection> controller_connection =
        connection_client_->Connect(*factory_.ControllerAddress(), 100);
    if (!controller_connection) {
      if (++attempts_to_connect_controller >
          kMaxAttemptsToConnectToController) {
        exit(1);
      } else {
        std::cerr << "Could not connect to the controller. Attempt "
                  << attempts_to_connect_controller << '\\'
                  << kMaxAttemptsToConnectToController << std::endl;
        return;
      }
    }
    if (!controller_connection->Write(m)) {
      if (++attempts_to_connect_controller > 6) {
        exit(1);
      } else {
        std::cerr << "Could not write to the controller. Attempt "
                  << attempts_to_connect_controller << '\\'
                  << kMaxAttemptsToConnectToController << std::endl;
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
    std::cerr << "Server " << connection_server_->address_str()
              << " read failed!" << std::endl;
    return;
  }
  switch (m.client_role) {
  case ClientRole::CONTROLLER: {
    if (m.type != MessageType::NEW_CLIENT) {
      std::cerr << "Server " << connection_server_->address_str()
                << " got incorrect message from controller!" << std::endl;
      return;
    }
    clients_.emplace(m.addresses[0]);
    return;
  } break;

  default:
    std::cerr << "Protocol error: server " << connection_server_->address_str()
              << " got message from incorrect node!" << std::endl;
    return;
  }
}
