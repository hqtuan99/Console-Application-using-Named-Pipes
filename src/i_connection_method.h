#ifndef I_CONNECTION_METHOD_H_
#define I_CONNECTION_METHOD_H_

#include <cstddef>
#include <memory>
#include <string>

#include "common.h"

const int kMaxAddressLength = 256;
const int kMaxNodes = 16;

enum class ClientRole { CLIENT, SERVER, CONTROLLER };

enum class MessageType { NEW_CLIENT, NEW_TIME, SET_SERVER, TEST_CONTROLLER };

struct Message {
  ClientRole client_role;
  MessageType type;
  TimePoint time;
  char addresses[kMaxAddressLength][kMaxNodes];
  int addresses_count;
  bool is_succeed;
};

class IAddress {
public:
  virtual ~IAddress() = default;
  virtual const std::string &raw() const = 0;
};

class IConnection {
public:
  virtual ~IConnection() = default;
  virtual bool Write(Message &) = 0;
  virtual Message Read() = 0;
  virtual void Close() = 0;
  virtual bool is_server() const = 0;
};

class IServer {
public:
  virtual ~IServer() = default;
  virtual std::unique_ptr<IConnection> WaitForConnection(int timeout) = 0;
  virtual const IAddress &address() const = 0;
  virtual const std::string &address_str() const = 0;
};

class IClient {
public:
  virtual ~IClient() = default;
  virtual std::unique_ptr<IConnection> Connect(const IAddress &address,
                                               int timeout) = 0;
};

class IConnectionMethodFactory {
public:
  virtual ~IConnectionMethodFactory() = default;
  virtual std::unique_ptr<IAddress> NewAddress(std::string address) = 0;
  virtual std::unique_ptr<IAddress> GenerateAddress() = 0;
  virtual std::unique_ptr<IServer> NewServer(const IAddress &address) = 0;
  virtual std::unique_ptr<IClient> NewClient() = 0;
  virtual std::unique_ptr<IAddress> ControllerAddress() = 0;
};

#endif // I_CONNECTION_METHOD_H_
