#include "pipe.h"

#include <assert.h>
#include <fileapi.h>
#include <windows.h>

#include <memory>
#include <string>

#include "common.h"

static const std::string kPipePrefix = "\\\\.\\pipe\\";

// PipeName

PipeName::PipeName(std::string name) {
  if (name.size() <= kPipePrefix.size() ||
      std::string(name.c_str(), kPipePrefix.size()) != kPipePrefix) {
    name_ = NewName(std::move(name));
  } else {
    name_ = std::move(name);
  }
}

PipeName::PipeName(const IAddress &address) : PipeName(address.raw()) {}

std::string PipeName::NewName(std::string raw_name) {
  if (raw_name.empty()) {
    return kPipePrefix + std::to_string(rand());
  } else if (raw_name[0] == '\\' && raw_name.size() > 1) {
    return kPipePrefix + raw_name.substr(1, raw_name.size() - 1);
  } else {
    return kPipePrefix + raw_name;
  }
}

// PipeConnection

PipeConnection::PipeConnection(HANDLE handle, bool is_server)
    : handle_(handle), is_server_(is_server) {}

PipeConnection::~PipeConnection() { Close(); }

bool PipeConnection::Write(Message &message) {
  DWORD bytes_written = 0;
  if (!WriteFile(handle_, &message, sizeof(Message), &bytes_written, nullptr)) {
    WriteLastErrorMessage("PipeConnection::Write");
    return false;
  }
  assert(bytes_written == sizeof(Message));
  return true;
}

Message PipeConnection::Read() {
  Message message;
  DWORD bytes_read = 0;
  OVERLAPPED overlapped{};
  bool is_succeed = true;
  if (!ReadFile(handle_, &message, sizeof(Message), &bytes_read, &overlapped) &&
      GetLastError() != ERROR_IO_PENDING) {
    WriteLastErrorMessage("PipeConnection::Read::ReadFile");
    is_succeed = false;
  }
  if (!GetOverlappedResult(handle_, &overlapped, &bytes_read, true)) {
    WriteLastErrorMessage("PipeConnection::Read::GetOverlappedResult");
    is_succeed = false;
  }
  assert(bytes_read == sizeof(Message));
  message.is_succeed = is_succeed;
  return message;
}

void PipeConnection::Close() {
  if (is_server_) {
    DisconnectNamedPipe(handle_);
  } else {
    // Assure that data was read by server
    Sleep(10);
    FlushFileBuffers(handle_);
    CloseHandle(handle_);
  }
}

// PipeServer

PipeServer::PipeServer(const IAddress &pipe_name) : pipe_name_(pipe_name) {
  handle_ = CreateNamedPipeA(pipe_name_,
                             FILE_FLAG_FIRST_PIPE_INSTANCE |
                                 PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                             PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
                             PIPE_UNLIMITED_INSTANCES, sizeof(Message),
                             sizeof(Message), 0, nullptr);
  if (handle_ == INVALID_HANDLE_VALUE) {
    WriteLastErrorMessage("PipeServer::CreateNamedPipe", pipe_name_);
    exit(1);
  }
}

PipeServer::~PipeServer() { CloseHandle(handle_); }

std::unique_ptr<IConnection> PipeServer::WaitForConnection(int timeout) {
  HANDLE connection_event = CreateEventA(nullptr, false, false, nullptr);
  if (!connection_event) {
    WriteLastErrorMessage("PipeServer::WaitForConnection::CreateEvent",
                          pipe_name_);
    return nullptr;
  }
  OVERLAPPED overlapped{};
  overlapped.hEvent = connection_event;
  ConnectNamedPipe(handle_, &overlapped);
  switch (GetLastError()) {
  // no client is ready
  case ERROR_IO_PENDING: {
    DWORD status = WaitForSingleObject(overlapped.hEvent, timeout);
    switch (status) {
    case WAIT_FAILED:
      WriteLastErrorMessage(
          "PipeServer::WaitForConnection::WaitForSingleObject", pipe_name_);
      return nullptr;
    case WAIT_TIMEOUT:
      // std::cerr << pipe_name_ << " timeout" << std::endl;
      return nullptr;
    case WAIT_OBJECT_0:
      return std::make_unique<PipeConnection>(handle_, true);
    }
  }
  // client is already connected
  case ERROR_PIPE_CONNECTED:
    return std::make_unique<PipeConnection>(handle_, true);
  default:
    WriteLastErrorMessage("PipeServer::WaitForConnection::ConnectNamedPipe",
                          pipe_name_);
    return nullptr;
  }
}

// PipeClient

std::unique_ptr<IConnection> PipeClient::Connect(const IAddress &address,
                                                 int timeout) {
  std::string address_string = address.raw();
  const char *const pipe_name = address_string.data();
  if (!WaitNamedPipeA(pipe_name, timeout)) {
    WriteLastErrorMessage("PipeClient::Connect::WaitNamedPipe", pipe_name);
    return nullptr;
  }
  HANDLE pipe_handle =
      CreateFileA(pipe_name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, nullptr);
  if (pipe_handle == INVALID_HANDLE_VALUE) {
    WriteLastErrorMessage("PipeClient::Connect::CreateFile", pipe_name);
    return nullptr;
  }
  return std::make_unique<PipeConnection>(pipe_handle, false);
}
