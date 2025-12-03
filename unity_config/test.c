#include "unity_internals.h"
#include <gg/error.h>
#include <gg/ipc/mock.h>
#include <gg/process_wait.h>
#include <gg/test.h>
#include <unistd.h>
#include <unity.h>
#include <stdlib.h>

#ifndef GG_TEST_SOCKET_DIR
#define GG_TEST_SOCKET_DIR "/tmp/gg-test"
#endif

#ifndef GG_TEST_AUTH_TOKEN
#define GG_TEST_AUTH_TOKEN "1234567890ABCDEF"
#endif

int server_handle = -1;
GgTestListNode *gg_test_list_head = NULL;

void gg_test_register(GgTestListNode *entry) {
    entry->next = gg_test_list_head;
    gg_test_list_head = entry;
}

__attribute__((weak)) void suiteSetUp(void) {
    fprintf(stderr, "Parent pid=%d\n", getpid());

    GgError ret = gg_test_setup_ipc(
        GG_TEST_SOCKET_DIR, 0777, &server_handle, GG_TEST_AUTH_TOKEN
    );
    if ((ret != GG_ERR_OK) || (server_handle < 0)) {
        _Exit(1);
    }
}

__attribute__((weak)) void setUp(void) {
}

__attribute__((weak)) void tearDown(void) {
    (void) gg_test_disconnect(server_handle);
}

__attribute__((weak)) int suiteTearDown(int num_failures) {
    gg_test_close(server_handle);
    server_handle = -1;
    return num_failures;
}

__attribute__((weak)) int gg_test_run_suite(void) {
    UNITY_BEGIN();

    GG_TEST_FOR_EACH(test) {
        UnitySetTestFile(test->func_file_name);

        pid_t pid = fork();
        if (pid < 0) {
            _Exit(1);
        }

        if (pid == 0) {
            suiteSetUp();
            UnitySetTestFile(test->func_file_name);
            UNITY_UINT failures_before = Unity.TestFailures;
            UnityDefaultTestRun(
                test->func, test->func_name, test->func_line_num
            );

            _Exit(suiteTearDown(failures_before == Unity.TestFailures ? 0 : 1));
        }

        Unity.CurrentTestName = test->func_name;
        Unity.CurrentTestLineNumber = test->func_line_num;
        Unity.NumberOfTests++;
        UNITY_CLR_DETAILS();
        UNITY_EXEC_TIME_START();
        GgError ret = gg_process_wait(pid);
        UNITY_EXEC_TIME_STOP();
        if (ret != GG_ERR_OK) {
            Unity.TestFailures += 1;
        }
    }

    return UNITY_END();
}
