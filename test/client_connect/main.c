
#include <errno.h>
#include <fcntl.h>
#include <ggl/arena.h>
#include <ggl/error.h>
#include <ggl/eventstream/rpc.h>
#include <ggl/file.h>
#include <ggl/ipc/client.h>
#include <ggl/ipc/mock.h>
#include <ggl/log.h>
#include <ggl/map.h>
#include <ggl/object.h>
#include <ggl/sdk.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unity.h>
#include <stdlib.h>

static int server_handle = -1;

#define GGL_TEST_ASSERT_OK(expr) TEST_ASSERT_EQUAL(GGL_ERR_OK, (expr))
#define GGL_TEST_ASSERT_BAD(expr) TEST_ASSERT_NOT_EQUAL(GGL_ERR_OK, (expr))

#define AUTH_TOKEN "0123456789ABCDEF"
#define SOCKET_PATH "connect_socket.ipc"

void suiteSetUp(void) {
    GglError ret
        = ggl_test_setup_ipc(SOCKET_PATH, 0777, &server_handle, AUTH_TOKEN);
    if ((ret != GGL_ERR_OK) || (server_handle < 0)) {
        TEST_ABORT();
    }
}

void setUp(void) {
}

void tearDown(void) {
}

int suiteTearDown(int num_failures) {
    ggl_test_close(server_handle);
    server_handle = -1;
    return num_failures;
}

static GglError ggl_process_wait(pid_t pid, bool *exit_status) {
    while (true) {
        siginfo_t info = { 0 };
        int ret = waitid(P_PID, (id_t) pid, &info, WEXITED);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            GGL_LOGE("Err %d when calling waitid.", errno);
            return GGL_ERR_FAILURE;
        }

        switch (info.si_code) {
        case CLD_EXITED:
            if (exit_status != NULL) {
                *exit_status = info.si_status == 0;
            }
            return GGL_ERR_OK;
        case CLD_KILLED:
        case CLD_DUMPED:
            if (exit_status != NULL) {
                *exit_status = false;
            }
            return GGL_ERR_OK;
        default:;
        }
    }
}

static GgipcPacket connect_packet(GglBuffer auth_token) {
    static EventStreamHeader headers[] = {
        { GGL_STR(":message-type"),
          { EVENTSTREAM_INT32, .int32 = EVENTSTREAM_CONNECT } },
        { GGL_STR(":message-flags"), { EVENTSTREAM_INT32, .int32 = 0 } },
        { GGL_STR(":stream-id"), { EVENTSTREAM_INT32, .int32 = 0 } },
        { GGL_STR(":version"),
          { EVENTSTREAM_STRING, .string = GGL_STR("0.1.0") } },
    };

    static GglKV payload[1];
    payload[0] = ggl_kv(GGL_STR("authToken"), ggl_obj_buf(auth_token));
    size_t payload_len = sizeof(payload) / sizeof(payload[0]);

    uint32_t header_count = sizeof(headers) / sizeof(headers[0]);
    return (GgipcPacket) { .direction = CLIENT_TO_SERVER,
                           .has_payload = true,
                           .payload = ggl_obj_map((GglMap) {
                               .pairs = payload, .len = payload_len }),
                           .headers = headers,
                           .header_count = header_count };
}

static GgipcPacket connect_ack_packet(void) {
    {
        static EventStreamHeader headers[] = {
            { GGL_STR(":message-type"),
              { EVENTSTREAM_INT32, .int32 = EVENTSTREAM_CONNECT_ACK } },
            { GGL_STR(":message-flags"),
              { EVENTSTREAM_INT32, .int32 = EVENTSTREAM_CONNECTION_ACCEPTED } },
            { GGL_STR(":stream-id"), { EVENTSTREAM_INT32, .int32 = 0 } },
        };
        uint32_t header_count = sizeof(headers) / sizeof(headers[0]);
        return (GgipcPacket) { .direction = SERVER_TO_CLIENT,
                               .has_payload = false,
                               .headers = headers,
                               .header_count = header_count };
    }
}

static GgipcPacketSequence ggl_test_conn_ack_sequence(GglBuffer auth_token) {
    static GgipcPacket packets[2];
    packets[0] = connect_packet(auth_token);
    packets[1] = connect_ack_packet();
    size_t len = sizeof(packets) / sizeof(packets[0]);
    return (GgipcPacketSequence) { .packets = packets, .len = len };
}

static GgipcPacketSequence ggl_test_connect_hangup_sequence(
    GglBuffer auth_token
) {
    static GgipcPacket packets[1];
    packets[0] = connect_packet(auth_token);
    size_t len = sizeof(packets) / sizeof(packets[0]);
    return (GgipcPacketSequence) { .packets = packets, .len = len };
}

static void test_ggl_connect_okay(void) {
    GgipcPacketSequence seq = ggl_test_conn_ack_sequence(GGL_STR(AUTH_TOKEN));

    pid_t pid = fork();
    if (pid < 0) {
        TEST_ABORT();
    }

    if (pid == 0) {
        ggl_sdk_init();
        GGL_TEST_ASSERT_OK(ggipc_connect());
        _Exit(0);
    }

    GGL_TEST_ASSERT_OK(ggl_test_expect_packet_sequence(seq, 30, server_handle));

    bool clean_exit = false;
    GGL_TEST_ASSERT_OK(ggl_process_wait(pid, &clean_exit));
    TEST_ASSERT_TRUE(clean_exit);
}

static void test_ggl_connect_with_token_okay(void) {
    GgipcPacketSequence seq = ggl_test_conn_ack_sequence(GGL_STR(AUTH_TOKEN));

    pid_t pid = fork();
    if (pid < 0) {
        TEST_ABORT();
    }

    if (pid == 0) {
        // make it impossible for client to get these env variables
        // done before SDK can make any threads
        // NOLINTBEGIN(concurrency-mt-unsafe)
        unsetenv("SVCUID");
        unsetenv("AWS_GG_NUCLEUS_DOMAIN_SOCKET_FILEPATH_FOR_COMPONENT");
        // NOLINTEND(concurrency-mt-unsafe)
        ggl_sdk_init();
        GGL_TEST_ASSERT_OK(
            ggipc_connect_with_token(GGL_STR(SOCKET_PATH), GGL_STR(AUTH_TOKEN))
        );
        _Exit(0);
    }

    GGL_TEST_ASSERT_OK(ggl_test_expect_packet_sequence(seq, 30, server_handle));

    bool clean_exit = false;
    GGL_TEST_ASSERT_OK(ggl_process_wait(pid, &clean_exit));
    TEST_ASSERT_TRUE(clean_exit);
}

static void test_ggl_connect_bad(void) {
    GgipcPacketSequence seq
        = ggl_test_connect_hangup_sequence(GGL_STR(AUTH_TOKEN));

    pid_t pid = fork();
    if (pid < 0) {
        TEST_ABORT();
    }

    if (pid == 0) {
        ggl_sdk_init();
        GGL_TEST_ASSERT_BAD(ggipc_connect());
        _Exit(0);
    }

    GGL_TEST_ASSERT_OK(ggl_test_expect_packet_sequence(seq, 30, server_handle));

    /// TODO: verify Classic behavior
    GGL_TEST_ASSERT_OK(ggl_test_disconnect(server_handle));

    bool clean_exit = false;
    GGL_TEST_ASSERT_OK(ggl_process_wait(pid, &clean_exit));
    TEST_ASSERT_TRUE(clean_exit);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    suiteSetUp();

    UNITY_BEGIN();
    RUN_TEST(test_ggl_connect_okay);
    RUN_TEST(test_ggl_connect_with_token_okay);
    RUN_TEST(test_ggl_connect_bad);
    int num_failures = UNITY_END();

    return suiteTearDown(num_failures);
}
