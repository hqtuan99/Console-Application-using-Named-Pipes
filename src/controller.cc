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
  LOG(kINFO) << "Controller is running...";
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
    if (m.type == MessageType::kTEST_CONTROLLER) {
      continue;
    }
    switch (m.client_role) {
    case ClientRole::kCLIENT: {
      switch (m.type) {
      case MessageType::kNEW_CLIENT: {
        LOG(kINFO) << "Got NEW_CLIENT from " << m.addresses[0];
        if (connected_nodes_addresses_.size() + 1 > kMaxNodes)
          LOG(kINFO) << "Too many nodes. Rejected!";
        connected_nodes_addresses_.emplace(m.addresses[0]);
      } break;
      default:
        LOG(kDEBUG) << "Protocol error: got incorrect message type from client";
        continue;
      }

    } break;
    case ClientRole::kSERVER: {
      switch (m.type) {
      case MessageType::kNEW_TIME: {
        last_server_response_ = Clock::now();
        LOG(kINFO) << "Got new time: "
                   << SerializeTimePoint(last_server_response_,
                                         "UTC: %Y-%m-%d %H:%M:%S");
        continue;
      } break;
      default:
        LOG(kDEBUG) << "Protocol error: got incorrect message type from server";
        continue;
      }
    } break;
    case ClientRole::kCONTROLLER: {
      LOG(kDEBUG) << "Error: got controller message in controller";
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
  LOG(kINFO) << "Server died or not set yet. Choosing new server...";
  while (!connected_nodes_addresses_.empty()) {
    auto supposed_new_server_it = connected_nodes_addresses_.begin();
    server_address_ = connection_factory_.NewAddress(*supposed_new_server_it);
    LOG(kINFO) << "Attempt to make " << server_address_->raw()
               << " to be a server";
    Message m;
    m.client_role = role;
    m.type = MessageType::kSET_SERVER;

    int i = 0;
    for (auto &client_address : connected_nodes_addresses_) {
      LOG(kINFO) << "Copying " << client_address << " to message";
      client_address.copy(m.addresses[i++], kMaxAddressLength);
    }
    m.addresses_count = i;

    {
      LOG(kINFO) << "Attempt to connect to " << server_address_->raw();
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
      LOG(kINFO) << "Successfully made " << server_address_->raw()
                 << " a server";
      return;
    }
  }
}