// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#pragma once

#include <vector>
#include <functional>

typedef std::vector<unsigned> ColumnWidths;

ColumnWidths CalculateColumns(const std::function<unsigned(size_t)>&& item_width_callback, size_t count, bool vertical, unsigned padding=2, unsigned max_width=79, unsigned max_columns=0xff);

