//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE();

template<typename TCell, pos_type TPos, size_type TSize>
class TwoDimensionalGrid
{
public:
    explicit TwoDimensionalGrid(TSize size) noexcept
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_VERIFY(size.width >= 0);
        FO_RUNTIME_VERIFY(size.height >= 0);

        _size = size;
    }

    TwoDimensionalGrid(const TwoDimensionalGrid&) = delete;
    TwoDimensionalGrid(TwoDimensionalGrid&&) noexcept = default;
    auto operator=(const TwoDimensionalGrid&) -> TwoDimensionalGrid& = delete;
    auto operator=(TwoDimensionalGrid&&) noexcept -> TwoDimensionalGrid& = default;
    virtual ~TwoDimensionalGrid() = default;

    [[nodiscard]] auto GetSize() const noexcept -> TSize { return _size; }
    [[nodiscard]] virtual auto GetCellForReading(TPos pos) const noexcept -> const TCell& = 0;
    [[nodiscard]] virtual auto GetCellForWriting(TPos pos) -> TCell& = 0;

    virtual void Resize(TSize size) = 0;

protected:
    TSize _size {};
};

template<typename TCell, pos_type TPos, size_type TSize>
class DynamicTwoDimensionalGrid final : public TwoDimensionalGrid<TCell, TPos, TSize>
{
    using base = TwoDimensionalGrid<TCell, TPos, TSize>;

public:
    explicit DynamicTwoDimensionalGrid(TSize size) noexcept :
        base(size)
    {
        FO_STACK_TRACE_ENTRY();
    }

    [[nodiscard]] auto GetCellForReading(TPos pos) const noexcept -> const TCell& override
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_VERIFY(base::_size.is_valid_pos(pos), _emptyCell);

        const auto it = _cells.find(pos);

        if (it == _cells.end()) {
            return _emptyCell;
        }
        else {
            return it->second;
        }
    }

    [[nodiscard]] auto GetCellForWriting(TPos pos) -> TCell& override
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(base::_size.is_valid_pos(pos));

        const auto it = _cells.find(pos);

        if (it == _cells.end()) {
            return _cells.emplace(pos, TCell {}).first->second;
        }
        else {
            return it->second;
        }
    }

    void Resize(TSize size) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(size.width >= 0);
        FO_RUNTIME_ASSERT(size.height >= 0);

        const auto prev_width = base::_size.width;
        const auto prev_height = base::_size.height;

        base::_size = size;

        for (int64 y = 0; y < std::max(prev_height, base::_size.height); y++) {
            for (int64 x = 0; x < std::max(prev_width, base::_size.width); x++) {
                if (x >= base::_size.width && y >= base::_size.height && x < prev_width && y < prev_height) {
                    const auto it = _cells.find(TPos {numeric_cast<decltype(std::declval<TPos>().x)>(x), numeric_cast<decltype(std::declval<TPos>().y)>(y)});

                    if (it != _cells.end()) {
                        _cells.erase(it);
                    }
                }
            }
        }
    }

private:
    unordered_map<TPos, TCell> _cells {};
    const TCell _emptyCell {};
};

template<typename TCell, pos_type TPos, size_type TSize>
class StaticTwoDimensionalGrid final : public TwoDimensionalGrid<TCell, TPos, TSize>
{
    using base = TwoDimensionalGrid<TCell, TPos, TSize>;

public:
    explicit StaticTwoDimensionalGrid(TSize size) noexcept :
        base(size)
    {
        FO_STACK_TRACE_ENTRY();

        const auto count = static_cast<size_t>(static_cast<int64>(base::_size.width) * base::_size.height);
        _preallocatedCells.resize(count);
    }

    [[nodiscard]] auto GetCellForReading(TPos pos) const noexcept -> const TCell& override
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_VERIFY(base::_size.is_valid_pos(pos), _emptyCell);

        const auto index = static_cast<size_t>(static_cast<int64>(pos.y) * base::_size.width + pos.x);
        auto& cell = _preallocatedCells[index];

        if (!cell) {
            return _emptyCell;
        }

        return *cell;
    }

    [[nodiscard]] auto GetCellForWriting(TPos pos) -> TCell& override
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(base::_size.is_valid_pos(pos));

        const auto index = numeric_cast<size_t>(static_cast<int64>(pos.y) * base::_size.width + pos.x);
        auto& cell = _preallocatedCells[index];

        if (!cell) {
            cell = SafeAlloc::MakeUnique<TCell>();
        }

        return *cell;
    }

    void Resize(TSize size) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(size.width >= 0);
        FO_RUNTIME_ASSERT(size.height >= 0);

        const auto prev_width = base::_size.width;
        const auto prev_height = base::_size.height;

        base::_size = size;

        vector<unique_ptr<TCell>> new_cells;
        const auto new_count = numeric_cast<size_t>(numeric_cast<int64>(base::_size.width) * base::_size.height);
        new_cells.resize(new_count);

        for (int64 y = 0; y < std::max(prev_height, base::_size.height); y++) {
            for (int64 x = 0; x < std::max(prev_width, base::_size.width); x++) {
                if (x < base::_size.width && y < base::_size.height && x < prev_width && y < prev_height) {
                    const auto new_index = numeric_cast<size_t>(y * base::_size.width + x);
                    const auto prev_index = numeric_cast<size_t>(y * prev_width + x);
                    new_cells[new_index] = std::move(_preallocatedCells[prev_index]);
                }
            }
        }

        _preallocatedCells = std::move(new_cells);
    }

private:
    vector<unique_ptr<TCell>> _preallocatedCells {};
    const TCell _emptyCell {};
};

FO_END_NAMESPACE();
