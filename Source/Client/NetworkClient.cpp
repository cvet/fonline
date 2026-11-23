// FOnline Engine
// SPDX-License-Identifier: MIT

#include "NetworkClient.h"

FO_BEGIN_NAMESPACE

NetworkClientConnection::NetworkClientConnection(ptr<ClientNetworkSettings> settings) :
    _settings {settings}
{
    _incomeBuf.resize(_settings->Network.NetBufferSize);
    _isConnecting = true;
}

auto NetworkClientConnection::CheckStatus(bool for_write) -> bool
{
    if (!_isConnecting && !_isConnected) {
        return false;
    }

    try {
        return CheckStatusImpl(for_write);
    }
    catch (...) {
        Disconnect();
        throw;
    }
}

auto NetworkClientConnection::SendData(const_span<uint8_t> buf) -> size_t
{
    if (!_isConnecting && !_isConnected) {
        return 0;
    }

    try {
        size_t send_len {SendDataImpl(buf)};
        _bytesSend += send_len;
        return send_len;
    }
    catch (...) {
        Disconnect();
        throw;
    }
}

auto NetworkClientConnection::ReceiveData() -> const_span<uint8_t>
{
    if (!_isConnecting && !_isConnected) {
        return {};
    }

    try {
        size_t recv_len {ReceiveDataImpl(_incomeBuf)};
        _bytesReceived += recv_len;

        if (recv_len == 0) {
            return {};
        }

        FO_VERIFY_AND_THROW(recv_len <= _incomeBuf.size(), "Received byte count exceeds the income buffer size");

        return {_incomeBuf.data(), recv_len};
    }
    catch (...) {
        Disconnect();
        throw;
    }
}

void NetworkClientConnection::Disconnect() noexcept
{
    if (!_isConnecting && !_isConnected) {
        return;
    }

    if (_isConnecting) {
        logging::write(logging::type::warning, "Can't connect to the server");

        _isConnecting = false;
    }

    if (_isConnected) {
        logging::write("Disconnect from the server");

        _isConnected = false;
    }

    DisconnectImpl();
}

FO_END_NAMESPACE
