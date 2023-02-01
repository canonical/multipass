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

#ifndef MULTIPASS_QEMUIMG_PROCESS_SPEC_H
#define MULTIPASS_QEMUIMG_PROCESS_SPEC_H

#include <multipass/process/process_spec.h>

#include <QString>
#include <QStringList>

namespace multipass
{

class QemuImgProcessSpec : public ProcessSpec
{
public:
    explicit QemuImgProcessSpec(const QStringList& args, const QString& source_image, const QString& target_image = {});

    QString program() const override;
    QStringList arguments() const override;

    QString apparmor_profile() const override;

private:
    const QStringList args;
    const QString source_image;
    const QString target_image;
};

} // namespace multipass

#endif // MULTIPASS_QEMUIMG_PROCESS_SPEC_H
