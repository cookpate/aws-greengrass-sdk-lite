#include <ggl/buffer.hpp>
#include <ggl/error.hpp>
#include <ggl/ipc/client.hpp>
#include <ggl/ipc/client_c_api.hpp>
#include <ggl/ipc/subscription.hpp>
#include <ggl/types.hpp>
#include <exception>
#include <functional>
#include <iostream>
#include <source_location>
#include <string_view>
#include <system_error>

namespace ggl::ipc {
extern "C" {
namespace {
    void subscribe_to_topic_callback(
        void *ctx,
        GglBuffer topic,
        GglObject payload,
        GgIpcSubscriptionHandle handle
    ) noexcept try {
        Subscription locked { handle };
        std::invoke(
            *static_cast<LocalTopicCallback *>(ctx),
            std::string_view { reinterpret_cast<char *>(topic.data),
                               topic.len },
            payload,
            locked
        );
        (void) locked.release();
    } catch (const std::exception &e) {
        std::cerr << "Exception caught in "
                  << std::source_location {}.function_name() << '\n'
                  << e.what() << '\n';
    } catch (...) {
        std::cerr << "Exception caught in "
                  << std::source_location {}.function_name() << '\n';
    }
}
}

// singleton interface class.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

std::error_code Client::subscribe_to_topic(
    std::string_view topic, LocalTopicCallback &callback, Subscription *handle
) noexcept {
    GgIpcSubscriptionHandle raw_handle;
    GglError ret = ggipc_subscribe_to_topic(
        ggl::Buffer { topic },
        subscribe_to_topic_callback,
        &callback,
        (handle != nullptr) ? &raw_handle : nullptr
    );
    if ((handle != nullptr) && (ret == GGL_ERR_OK)) {
        handle->reset(raw_handle);
    }
    return ret;
}

// NOLINTEND(readability-convert-member-functions-to-static)

}
