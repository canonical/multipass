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

#ifndef MULTIPASS_POCO_ZIP_UTILS_H
#define MULTIPASS_POCO_ZIP_UTILS_H

#include "singleton.h"

#include <Poco/Zip/ZipArchive.h>

#include <fstream>

#define MP_POCOZIPUTILS multipass::PocoZipUtils::instance()

namespace multipass
{
class PocoZipUtils : public Singleton<PocoZipUtils>
{
public:
    PocoZipUtils(const Singleton<PocoZipUtils>::PrivatePass&) noexcept;

    virtual Poco::Zip::ZipArchive zip_archive_for(std::ifstream& zip_stream) const;
};
} // namespace multipass

#endif // MULTIPASS_POCO_ZIP_UTILS_H
