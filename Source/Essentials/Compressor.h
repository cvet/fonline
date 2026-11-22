// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "BasicCore.h"
#include "Containers.h"
#include "ExceptionHandling.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(CompressionException);
FO_DECLARE_EXCEPTION(DecompressException);

namespace compressor
{
    [[nodiscard]] auto calculate_max_compressed_buf_size(size_t initial_size) noexcept -> size_t;
    [[nodiscard]] auto compress(const_span<uint8_t> data) -> vector<uint8_t>;
    [[nodiscard]] auto compress(const_span<uint8_t> data, int32_t level) -> vector<uint8_t>;
    [[nodiscard]] auto decompress(const_span<uint8_t> data, size_t mul_approx) -> vector<uint8_t>;
    [[nodiscard]] auto decompress_exact(const_span<uint8_t> data, size_t decoded_size) -> vector<uint8_t>;
}

class stream_compressor final
{
public:
    stream_compressor() noexcept;
    stream_compressor(const stream_compressor&) = delete;
    stream_compressor(stream_compressor&&) noexcept;
    auto operator=(const stream_compressor&) = delete;
    auto operator=(stream_compressor&&) noexcept -> stream_compressor&;
    ~stream_compressor();

    void compress(const_span<uint8_t> buf, vector<uint8_t>& result);
    void reset() noexcept;

private:
    struct impl;
    unique_nptr<impl> _impl {};
};

class stream_decompressor final
{
public:
    stream_decompressor() noexcept;
    stream_decompressor(const stream_decompressor&) = delete;
    stream_decompressor(stream_decompressor&&) noexcept;
    auto operator=(const stream_decompressor&) = delete;
    auto operator=(stream_decompressor&&) noexcept -> stream_decompressor&;
    ~stream_decompressor();

    void decompress(const_span<uint8_t> buf, vector<uint8_t>& result);
    void reset() noexcept;

private:
    struct impl;
    unique_nptr<impl> _impl {};
};

FO_END_NAMESPACE
