#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <nats/nats.h>

// Regex: ^natsOptions_[a-zA-Z0-9_]+
// ---------------------------------
// OK natsOptions_Create
// OK natsOptions_SetURL
// OK natsOptions_SetServers
// OK natsOptions_SetUserInfo
// OK natsOptions_SetToken
// natsOptions_SetTokenHandler
// OK natsOptions_SetNoRandomize
// OK natsOptions_SetTimeout
// OK natsOptions_SetName
// OK natsOptions_SetSecure
// OK natsOptions_LoadCATrustedCertificates
// OK natsOptions_SetCATrustedCertificates
// OK natsOptions_LoadCertificatesChain
// OK natsOptions_SetCertificatesChain
// OK natsOptions_SetCiphers
// OK natsOptions_SetCipherSuites
// OK natsOptions_SetExpectedHostname
// OK natsOptions_SkipServerVerification
// OK natsOptions_SetVerbose
// OK natsOptions_SetPedantic
// OK natsOptions_SetPingInterval
// OK natsOptions_SetMaxPingsOut
// OK natsOptions_SetIOBufSize
// OK natsOptions_SetAllowReconnect
// OK natsOptions_SetMaxReconnect
// OK natsOptions_SetReconnectWait
// OK natsOptions_SetReconnectJitter
// natsOptions_SetCustomReconnectDelay
// OK natsOptions_SetReconnectBufSize
// OK natsOptions_SetMaxPendingMsgs
// OK natsOptions_SetErrorHandler (used in NatsClient)
// OK natsOptions_SetClosedCB (used in NatsClient)
// OK natsOptions_SetDisconnectedCB (used in NatsClient)
// OK natsOptions_SetReconnectedCB (used in NatsClient)
// natsOptions_SetDiscoveredServersCB
// natsOptions_SetLameDuckModeCB
// natsOptions_SetIgnoreDiscoveredServers
// natsOptions_SetEventLoop
// OK natsOptions_UseGlobalMessageDelivery
// OK natsOptions_IPResolutionOrder
// OK natsOptions_SetSendAsap
// natsOptions_UseOldRequestStyle
// OK natsOptions_SetFailRequestsOnDisconnect
// OK natsOptions_SetNoEcho
// OK natsOptions_SetRetryOnFailedConnect (TODO some params missing)
// natsOptions_SetUserCredentialsCallbacks
// OK natsOptions_SetUserCredentialsFromFiles
// OK natsOptions_SetUserCredentialsFromMemory
// natsOptions_SetNKey
// natsOptions_SetNKeyFromSeed
// OK natsOptions_SetWriteDeadline
// OK natsOptions_DisableNoResponders
// OK natsOptions_SetCustomInboxPrefix
// OK natsOptions_SetMessageBufferPadding
// OK natsOptions_Destroy

namespace nats
{
using std::string;
using std::string_view;
using std::vector;

struct NatsOptions
{
    natsOptions* ptr = nullptr;
    natsStatus s;

    NatsOptions(natsOptions* opts) noexcept //
        : ptr(opts)
    {
    }

    ~NatsOptions() noexcept
    {
        if (ptr)
        {
            natsOptions_Destroy(ptr);
            ptr = nullptr;
        }
    }

    // Delete copy
    NatsOptions(const NatsOptions&) = delete;
    NatsOptions& operator=(const NatsOptions&) = delete;

    // Allow moving
    NatsOptions(NatsOptions&& other) noexcept : ptr(other.ptr), s(other.s)
    {
        other.ptr = nullptr;
    }
    NatsOptions& operator=(NatsOptions&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr)
            {
                natsOptions_Destroy(ptr);
                ptr = nullptr;
            }
            ptr = other.ptr;
            s = other.s;
            other.ptr = nullptr;
        }
        return *this;
    }

    NatsOptions& set_servers(const vector<string>& servers) noexcept
    {
        vector<const char*> servers_c;
        servers_c.reserve(servers.size());

        for (const auto& server : servers)
            servers_c.push_back(server.c_str());

        s = natsOptions_SetServers(ptr, servers_c.data(), servers_c.size());

        return *this;
    }

    NatsOptions& set_token(string_view token) noexcept
    {
        s = natsOptions_SetToken(ptr, token.data());
        return *this;
    }

    NatsOptions& set_name(string_view name) noexcept
    {
        s = natsOptions_SetName(ptr, name.data());
        return *this;
    }

    NatsOptions& set_no_randomize(bool no_randomize) noexcept
    {
        s = natsOptions_SetNoRandomize(ptr, no_randomize);
        return *this;
    }

    NatsOptions& set_secure(bool secure) noexcept
    {
        s = natsOptions_SetSecure(ptr, secure);
        return *this;
    }

    NatsOptions& load_ca_trusted_certificates(string_view file_name) noexcept
    {
        s = natsOptions_LoadCATrustedCertificates(ptr, file_name.data());
        return *this;
    }

    NatsOptions& set_ca_trusted_certificates(string_view certificates) noexcept
    {
        s = natsOptions_SetCATrustedCertificates(ptr, certificates.data());
        return *this;
    }

    NatsOptions& load_certificates_chain(string_view certs_file_name, string_view key_file_name) noexcept
    {
        s = natsOptions_LoadCertificatesChain(ptr, certs_file_name.data(), key_file_name.data());
        return *this;
    }

    NatsOptions& set_certificates_chain(string_view cert, string_view key) noexcept
    {
        s = natsOptions_SetCertificatesChain(ptr, cert.data(), key.data());
        return *this;
    }

    NatsOptions& set_ciphers(string_view ciphers) noexcept
    {
        s = natsOptions_SetCiphers(ptr, ciphers.data());
        return *this;
    }

    NatsOptions& set_cipher_suites(string_view ciphers) noexcept
    {
        s = natsOptions_SetCipherSuites(ptr, ciphers.data());
        return *this;
    }

    NatsOptions& set_timeout(int64_t timeout_ms) noexcept
    {
        s = natsOptions_SetTimeout(ptr, timeout_ms);
        return *this;
    }

    NatsOptions& set_url(string_view url) noexcept
    {
        s = natsOptions_SetURL(ptr, url.data());
        return *this;
    }

    NatsOptions& set_verbose(bool verbose) noexcept
    {
        s = natsOptions_SetVerbose(ptr, verbose);
        return *this;
    }

    NatsOptions& set_retry_on_failed_connect(bool retry) noexcept
    {
        // TODO: Add retry options
        s = natsOptions_SetRetryOnFailedConnect(ptr, retry, NULL, NULL);
        return *this;
    }

    NatsOptions& set_expected_hostname(string_view hostname) noexcept
    {
        s = natsOptions_SetExpectedHostname(ptr, hostname.data());
        return *this;
    }

    NatsOptions& skip_server_verification(bool skip) noexcept
    {
        s = natsOptions_SkipServerVerification(ptr, skip);
        return *this;
    }

    NatsOptions& set_pedantic(bool pedantic) noexcept
    {
        s = natsOptions_SetPedantic(ptr, pedantic);
        return *this;
    }

    NatsOptions& set_ping_interval(int64_t interval_ms) noexcept
    {
        s = natsOptions_SetPingInterval(ptr, interval_ms);
        return *this;
    }

    NatsOptions& set_max_pings_out(int max_pings_out) noexcept
    {
        s = natsOptions_SetMaxPingsOut(ptr, max_pings_out);
        return *this;
    }

    NatsOptions& set_io_buffer_size(int io_buffer_size) noexcept
    {
        s = natsOptions_SetIOBufSize(ptr, io_buffer_size);
        return *this;
    }

    NatsOptions& set_allow_reconnect(bool allow_reconnect) noexcept
    {
        s = natsOptions_SetAllowReconnect(ptr, allow_reconnect);
        return *this;
    }

    NatsOptions& set_max_reconnect(int max_reconnect) noexcept
    {
        s = natsOptions_SetMaxReconnect(ptr, max_reconnect);
        return *this;
    }

    NatsOptions& set_reconnect_wait(int64_t wait_ms) noexcept
    {
        s = natsOptions_SetReconnectWait(ptr, wait_ms);
        return *this;
    }

    NatsOptions& set_reconnect_jitter(int64_t jitter_ms, int64_t jitter_tls_ms) noexcept
    {
        s = natsOptions_SetReconnectJitter(ptr, jitter_ms, jitter_tls_ms);
        return *this;
    }

    NatsOptions& set_reconnect_buf_size(int reconnect_buffer_size) noexcept
    {
        s = natsOptions_SetReconnectBufSize(ptr, reconnect_buffer_size);
        return *this;
    }

    NatsOptions& set_max_pending_msgs(int max_pending) noexcept
    {
        s = natsOptions_SetMaxPendingMsgs(ptr, max_pending);
        return *this;
    }

    NatsOptions& set_send_asap(bool send_asap) noexcept
    {
        s = natsOptions_SetSendAsap(ptr, send_asap);
        return *this;
    }

    NatsOptions& set_fail_requests_on_disconnect(bool fail_requests) noexcept
    {
        s = natsOptions_SetFailRequestsOnDisconnect(ptr, fail_requests);
        return *this;
    }

    NatsOptions& set_no_echo(bool no_echo) noexcept
    {
        s = natsOptions_SetNoEcho(ptr, no_echo);
        return *this;
    }

    NatsOptions& set_write_deadline(bool deadline_ms) noexcept
    {
        s = natsOptions_SetWriteDeadline(ptr, deadline_ms);
        return *this;
    }

    NatsOptions& disable_no_responders(bool disabled) noexcept
    {
        s = natsOptions_DisableNoResponders(ptr, disabled);
        return *this;
    }

    NatsOptions& set_custom_inbox_prefix(string_view prefix) noexcept
    {
        s = natsOptions_SetCustomInboxPrefix(ptr, prefix.data());
        return *this;
    }

    NatsOptions& set_message_buffer_padding(int padding_size) noexcept
    {
        s = natsOptions_SetMessageBufferPadding(ptr, padding_size);
        return *this;
    }

    NatsOptions& set_user_info(string_view username, string_view password) noexcept
    {
        s = natsOptions_SetUserInfo(ptr, username.data(), password.data());
        return *this;
    }

    NatsOptions& use_global_message_delivery(bool global) noexcept
    {
        s = natsOptions_UseGlobalMessageDelivery(ptr, global);
        return *this;
    }

    NatsOptions& ip_resolution_order(int order) noexcept
    {
        s = natsOptions_IPResolutionOrder(ptr, order);
        return *this;
    }

    NatsOptions& set_user_credentials_from_file(string_view user_or_chained_file, string_view seed_file) noexcept
    {
        s = natsOptions_SetUserCredentialsFromFiles(ptr, user_or_chained_file.data(), seed_file.data());
        return *this;
    }

    NatsOptions& set_user_credentials_from_memory(string_view jwt_and_seed_content) noexcept
    {
        s = natsOptions_SetUserCredentialsFromMemory(ptr, jwt_and_seed_content.data());
        return *this;
    }

};
}; // namespace nats
