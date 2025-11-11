#ifndef GGL_IPC_MOCK_HPP
#define GGL_IPC_MOCK_HPP

#include <cstddef>
extern "C" {
#include <ggl/attr.h>
#include <sys/types.h>
}
#include <ggl/ipc/mock_types.hpp>
#include <ggl/types.hpp>

extern "C" {

typedef enum : int8_t {
    CLIENT_TO_SERVER = 0,
    SERVER_TO_CLIENT = 1
} GgipcPacketDirection;

/// Sets up the IPC mock for this thread, opening a socket at path with mode,
/// and setting IPC environment variables. The IPC mock will not accept
/// connections until ggl_test_expect_packet_sequence() is called.
NULL_TERMINATED_STRING_ARG(1) NONNULL(3) NULL_TERMINATED_STRING_ARG(4)
GglError ggl_test_setup_ipc(
    const char *path, mode_t mode, int *handle, const char *auth_token
) noexcept;

typedef struct {
    EventStreamHeader *headers;
    uint32_t header_count;
    /// client->server packets must be GglMap.
    GglObject payload;
    /// empty payload "{}" is semantically-different from no payload "".
    bool has_payload;
    int direction; // 0 = client->server, 1 = server->client
} GgipcPacket;

typedef struct {
    GgipcPacket *packets;
    size_t len;
} GgipcPacketSequence;

/// Blocks until all packets in the packet sequence are validated or until any
/// are invalidated. Returns GGL_ERR_OK if all packets were successfully
/// sent/received. The client timeout is the time in seconds to wait for the
/// next client packet before failing with GGL_ERR_TIMEOUT.
GglError ggl_test_expect_packet_sequence(
    GgipcPacketSequence sequence, int client_timeout, int handle
) noexcept;

/// Hangs up on the client
GglError ggl_test_disconnect(int handle) noexcept;

void ggl_test_close(int handle) noexcept;
}

#endif
