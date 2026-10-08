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

#pragma once

#include <multipass/rpc/multipass.pb.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace multipass::test
{

::testing::Matcher<ImagesReply> Images(const auto& inner_matcher)
{
    return ::testing::Property(&ImagesReply::images_info, inner_matcher);
}

::testing::Matcher<ImagesReply_ImageInfo> ImageAliases(const auto& inner_matcher)
{
    return ::testing::Property(&ImagesReply_ImageInfo::aliases, inner_matcher);
}

::testing::Matcher<ImagesReply_ImageInfo> ImageRelease(const auto& inner_matcher)
{
    return ::testing::Property(&ImagesReply_ImageInfo::release, inner_matcher);
}

::testing::Matcher<ImagesReply_ImageInfo> ImageRemote(const auto& inner_matcher)
{
    return ::testing::Property(&ImagesReply_ImageInfo::remote_name, inner_matcher);
}

::testing::Matcher<ImagesReply_ImageInfo> ImageHash(const auto& inner_matcher)
{
    return ::testing::Property(&ImagesReply_ImageInfo::hash, inner_matcher);
}

::testing::Matcher<ImagesReply> AllRemotesFlag(const auto& inner_matcher)
{
    return ::testing::Property(&ImagesReply::all_remotes, inner_matcher);
}

} // namespace multipass::test
