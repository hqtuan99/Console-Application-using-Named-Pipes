#ifndef PIPE_H_
#define PIPE_H_

#include <string>

#include "i_connection_method.h"

class PipeName : public IAddress {
public:
  PipeName(std::string name);
  PipeName(const IAddress &address);
  const std::string &raw() const override { return name_; }
  operator const char * const() const { return name_.c_str(); }

private:
  static std::string NewName(std::string raw_name = "");
  std::string name_;
};

class PipeConnection : public IConnection {
public:
  explicit PipeConnection(HANDLE handle, bool is_server);
  ~PipeConnection() override;

  bool Write(Message &message) override;
  Message Read() override;
  void Close() override;
  bool is_server() const override { return is_server_; }

private:
  HANDLE handle_;
  bool is_server_;
};

class PipeServer : public IServer {
public:
  explicit PipeServer(const IAddress &pipe_name = PipeName(""));
  ~PipeServer() override;
  PipeServer(const PipeServer &) = delete;
  PipeServer &operator=(const PipeServer &) = delete;
  PipeServer(PipeServer &&) = delete;
  PipeServer &operator=(PipeServer &&) = delete;

  std::unique_ptr<IConnection> WaitForConnection(int timeout) override;

  const IAddress &address() const override { return pipe_name_; }
  const std::string &address_str() const override { return pipe_name_.raw(); }

private:
  PipeName pipe_name_;
  HANDLE handle_;
};

class PipeClient : public IClient {
public:
  std::unique_ptr<IConnection> Connect(const IAddress &address,
                                       int timeout) override;
};

class PipeFactory : public IConnectionMethodFactory {
public:
  std::unique_ptr<IAddress> GenerateAddress() override {
    return std::make_unique<PipeName>("");
  }
  std::unique_ptr<IAddress> NewAddress(std::string address) override {
    return std::make_unique<PipeName>(address);
  }
  std::unique_ptr<IServer> NewServer(const IAddress &address) override {
    return std::make_unique<PipeServer>(address);
  }
  std::unique_ptr<IClient> NewClient() override {
    return std::make_unique<PipeClient>();
  }
  std::unique_ptr<IAddress> ControllerAddress() override {
    return NewAddress("\\\\.\\pipe\\controller");
  }
};

#endif // PIPE_H_
