/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "process_spec.h"

namespace multipass
{

class QemuImgProcessSpec : public ProcessSpec
{
public:
    explicit QemuImgProcessSpec(const QString& input_image_path, const QString& output_image_path = QString());

    QString program() const override;
    QString apparmor_profile() const override;

    void setup_child_process() const override;

private:
    const QString input_image_path;
    const QString output_image_path;
    const QString executable;
};

} // namespace multipass

#endif // MULTIPASS_QEMUIMG_PROCESS_SPEC_H
