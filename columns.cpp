﻿// Copyright (c) 2024 by Christopher Antos
// License: http://opensource.org/licenses/MIT

// vim: set et ts=4 sw=4 cino={0s:

#include "pch.h"
#include "columns.h"

#include <assert.h>

ColumnWidths CalculateColumns(const std::function<unsigned(size_t)>&& item_width_callback, const size_t count, const bool vertical, const unsigned padding, unsigned max_width, unsigned max_columns)
{
    ColumnWidths out;

    if (count > 0)
    {
        if (max_columns > count)
            max_columns = unsigned(count);
        if (max_columns > 50)
            max_columns = 50;
        if (max_width > 1024)
            max_width = 1024;

        // Memory layout:
        //
        //      NUM_COLS    COL_1       COL_2       COL_3   ...
        //      --------    -----       -----       -----
        //      VALID  1       61
        //        -    2       61          45
        //      VALID  3       12          61           5
        //       ...

        struct CandidateColumns
        {
            bool            valid;
            unsigned        line_width;
            unsigned*       column_widths;
            size_t          vertical_stride;
        };

        CandidateColumns* candidates = new CandidateColumns[max_columns];
        unsigned* width_storage = new unsigned[(max_columns * (max_columns + 1)) / 2];

        if (candidates && width_storage)
        {
            // Initialize the data structures.
            unsigned* storage = width_storage;
            for (unsigned y = 0; y < max_columns; ++y)
            {
                auto& candidate = candidates[y];
                candidate.valid = true;
                candidate.line_width = (y * (1 + padding)) + 1;
                for (unsigned x = 0; x <= y; ++x)
                {
                    *storage = 1; // Empty columns aren't supported.
                    candidate.column_widths = storage++;
                }
                candidate.vertical_stride = (count + y) / (y + 1);
            }

            // Evaluate the item widths.
            for (size_t i = 0; i < count; ++i)
            {
                // For each candidate number of columns...
                unsigned new_max = 0;
                for (unsigned n = 0; n < max_columns; ++n)
                {
                    // Skip invalid candidates.
                    auto& candidate = candidates[n];
                    if (!candidate.valid)
                        continue;

                    // Compute the item's column index in this candidate.
                    unsigned c;
                    if (vertical)
                        c = unsigned(i / candidate.vertical_stride);
                    else
                        c = unsigned(i % (n + 1));

                    // Update the column's width.
                    const unsigned col_width = candidate.column_widths[c];
                    const unsigned item_width = item_width_callback(i);
                    if (col_width < item_width)
                    {
                        const unsigned line_width = candidate.line_width - col_width + item_width;
                        if (line_width > max_width && n)
                        {
                            candidate.valid = false;
                            continue;
                        }
                        candidate.line_width = line_width;
                        candidate.column_widths[c] = item_width;
                    }

                    new_max = n + 1;
                }

                max_columns = new_max;
            }

            assert(max_columns > 0);
            assert(candidates[max_columns - 1].valid);
            assert(!max_columns || candidates[max_columns - 1].line_width <= max_width);

            for (const unsigned* col_widths = candidates[max_columns - 1].column_widths; max_columns--;)
                out.emplace_back(*(col_widths++));
        }

        delete [] candidates;
        delete [] width_storage;
    }

    return out;
}

