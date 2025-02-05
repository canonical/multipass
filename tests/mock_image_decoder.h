/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MULTIPASS_MOCK_IMAGE_DECODER_H
#define MULTIPASS_MOCK_IMAGE_DECODER_H

#include "common.h"

#include <multipass/path.h>
#include <multipass/progress_monitor.h>

namespace multipass::test
{

class MockImageDecoder
{
public:
    MOCK_METHOD(void, decode_to, (const Path&, const Path&, const ProgressMonitor&), (const));
};

} // namespace multipass::test

#endif // MULTIPASS_MOCK_IMAGE_DECODER_H
