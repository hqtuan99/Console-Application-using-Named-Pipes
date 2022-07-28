#include "controller.h"

#include <iostream>
#include <memory>

using namespace std::chrono_literals;
const Clock::duration Controller::max_server_response = 6s;

Controller::Controller(IConnectionMethodFactory &factory)
    : connection_factory_(factory),
      connection_server_(factory.NewServer(*factory.ControllerAddress())),
      connection_client_(factory.NewClient()) {}

void Controller::Run() {
  std::cout << "Controller is running..." << std::endl;
  while (true) {
    std::unique_ptr<IConnection> connection =
        connection_server_->WaitForConnection(5000);
    if (!connection) {
      ChooseNewServer();
      if (connected_nodes_addresses_.empty())
        return;
      else
        continue;
    }
    Message m = connection->Read();
    connection->Close();
    if (!m.is_succeed) {
      continue;
      // error
    }
    if (m.type == MessageType::TEST_CONTROLLER) {
      continue;
    }
    switch (m.client_role) {
    case ClientRole::CLIENT: {
      switch (m.type) {
      case MessageType::NEW_CLIENT: {
        std::cout << "Got NEW_CLIENT from " << m.addresses[0] << std::endl;
        if (connected_nodes_addresses_.size() + 1 > kMaxNodes)
          std::cout << "Too many nodes. Rejected!" << std::endl;
        connected_nodes_addresses_.emplace(m.addresses[0]);
      } break;
      default:
        std::cerr << "Protocol error: got incorrect message type from client"
                  << std::endl;
        continue;
      }

    } break;
    case ClientRole::SERVER: {
      switch (m.type) {
      case MessageType::NEW_TIME: {
        last_server_response_ = Clock::now();
        std::cout << "Got new time: "
                  << serializeTimePoint(last_server_response_,
                                        "UTC: %Y-%m-%d %H:%M:%S")
                  << std::endl;
        continue;
      } break;
      default:
        std::cerr << "Protocol error: got incorrect message type from server"
                  << std::endl;
        continue;
      }
    } break;
    case ClientRole::CONTROLLER: {
      std::cerr << "Error: got controller message in controller" << std::endl;
      continue;
    } break;
    }
    // if we are here, we got message NEW_CLIENT from CLIENT
    if (Clock::now() - last_server_response_ > max_server_response) {
      // server was not responding too many time
      ChooseNewServer();
      if (connected_nodes_addresses_.empty()) {
        return;
      }
    }
    // send message to server to add new client to it
    bool was_server_acknowledgment_succeed = false;
    while (!was_server_acknowledgment_succeed &&
           !connected_nodes_addresses_.empty()) {
      std::unique_ptr<IConnection> server_connection =
          connection_client_->Connect(*server_address_, 1000);

      if (!server_connection) {
        // could not make connection to server
        ChooseNewServer();
        continue;
      }

      m.client_role = role;
      if (!server_connection->Write(m)) {
        // connection to server lost
        server_connection->Close();
        ChooseNewServer();
        continue;
      }
      was_server_acknowledgment_succeed = true;
      server_connection->Close();
    }
    if (!was_server_acknowledgment_succeed)
      return;
  }
}

void Controller::ChooseNewServer() {
  std::cout << "Server died or not set yet. Choosing new server..."
            << std::endl;
  while (!connected_nodes_addresses_.empty()) {
    auto supposed_new_server_it = connected_nodes_addresses_.begin();
    server_address_ = connection_factory_.NewAddress(*supposed_new_server_it);
    std::cout << "Attempt to make " << server_address_->raw()
              << " to be a server" << std::endl;
    Message m;
    m.client_role = role;
    m.type = MessageType::SET_SERVER;

    int i = 0;
    for (auto &client_address : connected_nodes_addresses_) {
      std::cout << "Copying " << client_address << " to message" << std::endl;
      client_address.copy(m.addresses[i++], kMaxAddressLength);
    }
    m.addresses_count = i;

    {
      std::cout << "Attempt to connect to " << server_address_->raw()
                << std::endl;
      std::unique_ptr<IConnection> new_server_connection =
          connection_client_->Connect(*server_address_, 100);
      if (!new_server_connection) {
        connected_nodes_addresses_.erase(supposed_new_server_it);
        continue;
      }
      if (!new_server_connection->Write(m)) {
        connected_nodes_addresses_.erase(supposed_new_server_it);
        continue;
      }
      std::cout << "Successfully made " << server_address_->raw() << " a server"
                << std::endl;
      return;
    }
  }
}