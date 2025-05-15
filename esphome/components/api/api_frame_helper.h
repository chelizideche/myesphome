#pragma once
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

#include "esphome/core/defines.h"
#ifdef USE_API
#ifdef USE_API_NOISE
#include "noise/protocol.h"
#endif

#include "api_noise_context.h"
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace api {

struct ReadPacketBuffer {
  std::vector<uint8_t> container;
  uint16_t type;
  size_t data_offset;
  size_t data_len;
};

struct PacketBuffer {
  const std::vector<uint8_t> container;
  uint16_t type;
  uint8_t data_offset;
  uint8_t data_len;
};

enum class APIError : int {
  OK = 0,
  WOULD_BLOCK = 1001,
  BAD_HANDSHAKE_PACKET_LEN = 1002,
  BAD_INDICATOR = 1003,
  BAD_DATA_PACKET = 1004,
  TCP_NODELAY_FAILED = 1005,
  TCP_NONBLOCKING_FAILED = 1006,
  CLOSE_FAILED = 1007,
  SHUTDOWN_FAILED = 1008,
  BAD_STATE = 1009,
  BAD_ARG = 1010,
  SOCKET_READ_FAILED = 1011,
  SOCKET_WRITE_FAILED = 1012,
  HANDSHAKESTATE_READ_FAILED = 1013,
  HANDSHAKESTATE_WRITE_FAILED = 1014,
  HANDSHAKESTATE_BAD_STATE = 1015,
  CIPHERSTATE_DECRYPT_FAILED = 1016,
  CIPHERSTATE_ENCRYPT_FAILED = 1017,
  OUT_OF_MEMORY = 1018,
  HANDSHAKESTATE_SETUP_FAILED = 1019,
  HANDSHAKESTATE_SPLIT_FAILED = 1020,
  BAD_HANDSHAKE_ERROR_BYTE = 1021,
  CONNECTION_CLOSED = 1022,
};

const char *api_error_to_str(APIError err);

class APIFrameHelper {
 public:
  APIFrameHelper() = default;
  explicit APIFrameHelper(std::unique_ptr<socket::Socket> socket) : socket_owned_(std::move(socket)) {
    socket_ = socket_owned_.get();
  }
  virtual ~APIFrameHelper() = default;
  virtual APIError init() = 0;
  virtual APIError loop() = 0;
  virtual APIError read_packet(ReadPacketBuffer *buffer) = 0;
  bool can_write_without_blocking() {
    // First check if we're in the DATA state
    if (state_ != State::DATA) {
      return false;
    }

    // Empty buffer can always accept more data
    if (tx_buf_.empty()) {
      return true;
    }

    // Optimization: Allow writing even with a small buffer backlog to reduce delays in message processing.
    // This improves throughput for real-time data like sensor readings and prevents high-priority
    // messages from being unnecessarily delayed by a small queue backlog.
    // The 256-byte threshold is small enough to not impact memory usage significantly
    // but large enough to improve overall system responsiveness.
    if (tx_buf_.size() == 1 && tx_buf_.front().remaining() < 256) {
      return true;
    }

    return false;
  }
  virtual APIError write_packet(uint16_t type, const uint8_t *data, size_t len) = 0;
  std::string getpeername() { return socket_->getpeername(); }
  int getpeername(struct sockaddr *addr, socklen_t *addrlen) { return socket_->getpeername(addr, addrlen); }
  APIError close() {
    state_ = State::CLOSED;
    int err = this->socket_->close();
    if (err == -1)
      return APIError::CLOSE_FAILED;
    return APIError::OK;
  }
  APIError shutdown(int how) {
    int err = this->socket_->shutdown(how);
    if (err == -1)
      return APIError::SHUTDOWN_FAILED;
    if (how == SHUT_RDWR) {
      state_ = State::CLOSED;
    }
    return APIError::OK;
  }
  // Give this helper a name for logging
  void set_log_info(std::string info) { info_ = std::move(info); }

 protected:
  // Struct for holding parsed frame data
  struct ParsedFrame {
    std::vector<uint8_t> msg;
  };

  // Buffer containing data to be sent
  struct SendBuffer {
    std::vector<uint8_t> data;
    uint16_t offset{0};  // Current offset within the buffer (uint16_t to reduce memory usage)

    // Using uint16_t reduces memory usage since ESPHome API messages are limited to 64KB max
    uint16_t remaining() const { return static_cast<uint16_t>(data.size()) - offset; }
    const uint8_t *current_data() const { return data.data() + offset; }
  };

  // Queue of data buffers to be sent
  std::deque<SendBuffer> tx_buf_;

  // Common state enum for all frame helpers
  // Note: Not all states are used by all implementations
  // - INITIALIZE: Used by both Noise and Plaintext
  // - CLIENT_HELLO, SERVER_HELLO, HANDSHAKE: Only used by Noise protocol
  // - DATA: Used by both Noise and Plaintext
  // - CLOSED: Used by both Noise and Plaintext
  // - FAILED: Used by both Noise and Plaintext
  // - EXPLICIT_REJECT: Only used by Noise protocol
  enum class State {
    INITIALIZE = 1,
    CLIENT_HELLO = 2,  // Noise only
    SERVER_HELLO = 3,  // Noise only
    HANDSHAKE = 4,     // Noise only
    DATA = 5,
    CLOSED = 6,
    FAILED = 7,
    EXPLICIT_REJECT = 8,  // Noise only
  };

  // Current state of the frame helper
  State state_{State::INITIALIZE};

  // Helper name for logging
  std::string info_;

  // Socket for communication
  socket::Socket *socket_{nullptr};
  std::unique_ptr<socket::Socket> socket_owned_;

  // Common implementation for writing raw data to socket
  APIError write_raw_(const struct iovec *iov, int iovcnt);

  // Try to send data from the tx buffer
  APIError try_send_tx_buf_();

  // Helper method to buffer data from IOVs
  void buffer_data_from_iov_(const struct iovec *iov, int iovcnt, size_t total_write_len);
};

#ifdef USE_API_NOISE
class APINoiseFrameHelper : public APIFrameHelper {
 public:
  APINoiseFrameHelper(std::unique_ptr<socket::Socket> socket, std::shared_ptr<APINoiseContext> ctx)
      : APIFrameHelper(std::move(socket)), ctx_(std::move(ctx)) {}
  ~APINoiseFrameHelper() override;
  APIError init() override;
  APIError loop() override;
  APIError read_packet(ReadPacketBuffer *buffer) override;
  APIError write_packet(uint16_t type, const uint8_t *payload, size_t len) override;

 protected:
  APIError state_action_();
  APIError try_read_frame_(ParsedFrame *frame);
  APIError write_frame_(const uint8_t *data, size_t len);
  APIError init_handshake_();
  APIError check_handshake_finished_();
  void send_explicit_handshake_reject_(const std::string &reason);
  // Fixed-size header buffer for noise protocol:
  // 1 byte for indicator + 2 bytes for message size (16-bit value, not varint)
  // Note: Maximum message size is 65535, with a limit of 128 bytes during handshake phase
  uint8_t rx_header_buf_[3];
  size_t rx_header_buf_len_ = 0;
  std::vector<uint8_t> rx_buf_;
  size_t rx_buf_len_ = 0;

  std::vector<uint8_t> prologue_;

  std::shared_ptr<APINoiseContext> ctx_;
  NoiseHandshakeState *handshake_{nullptr};
  NoiseCipherState *send_cipher_{nullptr};
  NoiseCipherState *recv_cipher_{nullptr};
  NoiseProtocolId nid_;
};
#endif  // USE_API_NOISE

#ifdef USE_API_PLAINTEXT
class APIPlaintextFrameHelper : public APIFrameHelper {
 public:
  APIPlaintextFrameHelper(std::unique_ptr<socket::Socket> socket) : APIFrameHelper(std::move(socket)) {}
  ~APIPlaintextFrameHelper() override = default;
  APIError init() override;
  APIError loop() override;
  APIError read_packet(ReadPacketBuffer *buffer) override;
  APIError write_packet(uint16_t type, const uint8_t *payload, size_t len) override;

 protected:
  APIError try_read_frame_(ParsedFrame *frame);
  // Fixed-size header buffer for plaintext protocol:
  // We only need space for the two varints since we validate the indicator byte separately.
  // To match noise protocol's maximum message size (65535), we need:
  // 3 bytes for message size varint (supports up to 2097151) + 2 bytes for message type varint
  //
  // While varints could theoretically be up to 10 bytes each for 64-bit values,
  // attempting to process messages with headers that large would likely crash the
  // ESP32 due to memory constraints.
  uint8_t rx_header_buf_[5];  // 5 bytes for varints (3 for size + 2 for type)
  uint8_t rx_header_buf_pos_ = 0;
  bool rx_header_parsed_ = false;
  uint32_t rx_header_parsed_type_ = 0;
  uint32_t rx_header_parsed_len_ = 0;

  std::vector<uint8_t> rx_buf_;
  size_t rx_buf_len_ = 0;
};
#endif

}  // namespace api
}  // namespace esphome
#endif
