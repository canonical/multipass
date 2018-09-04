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

#include <multipass/metrics/metrics_provider.h>
#include <multipass/utils.h>

#include "temp_dir.h"
#include "temp_file.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUuid>

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace std::literals::chrono_literals;
using namespace testing;

namespace
{
struct MetricsProvider : public testing::Test
{
    void wait_for_metrics()
    {
        while (true)
        {
            QFile file{metrics_file.name()};

            if (file.size() == 0 || !file.open(QIODevice::ReadWrite))
            {
                std::this_thread::sleep_for(10ms);
            }
            else
            {
                break;
            }
        }
    }

    mpt::TempFile metrics_file;
    mpt::TempDir metrics_dir;
    mp::MetricsData metrics_data;
};
} // namespace

TEST_F(MetricsProvider, opt_in_metrics_valid)
{
    const auto unique_id = mp::utils::make_uuid();
    mp::MetricsProvider metrics_provider{metrics_file.url(), unique_id, metrics_dir.path()};
    metrics_provider.send_metrics(metrics_data);

    wait_for_metrics();

    QFile file{metrics_file.name()};
    file.open(QIODevice::ReadOnly);

    auto data = file.readAll();
    auto json = QJsonDocument::fromJson(data);

    EXPECT_FALSE(json.isNull());
    EXPECT_TRUE(json.isArray());

    auto metric_batches = json.array();

    for (const auto& metric_batch : metric_batches)
    {
        EXPECT_TRUE(metric_batch.isObject());

        const auto batch_obj = metric_batch.toObject();
        EXPECT_THAT(batch_obj.size(), Eq(4));
        EXPECT_TRUE(batch_obj.contains("uuid"));
        EXPECT_TRUE(batch_obj.contains("created"));
        EXPECT_TRUE(batch_obj.contains("credentials"));
        EXPECT_TRUE(batch_obj.contains("metrics"));

        // Ensure 'uuid' has a valid UUID
        EXPECT_FALSE(QUuid(batch_obj["uuid"].toString()).isNull());
        // Ensure 'created' has a valid RFC3339 timestamp
        EXPECT_TRUE(QDateTime::fromString(batch_obj["created"].toString(), Qt::ISODateWithMs).isValid());

        EXPECT_TRUE(batch_obj["metrics"].isArray());
        auto metrics = batch_obj["metrics"].toArray();

        for (const auto& metric : metrics)
        {
            EXPECT_TRUE(metric.isObject());

            const auto metric_obj = metric.toObject();
            EXPECT_THAT(metric_obj.size(), Eq(4));
            EXPECT_TRUE(metric_obj.contains("key"));
            EXPECT_TRUE(metric_obj.contains("value"));
            EXPECT_TRUE(metric_obj.contains("time"));
            EXPECT_TRUE(metric_obj.contains("tags"));

            EXPECT_THAT(metric_obj["key"].toString().toStdString(), Eq("host-machine-info"));
            EXPECT_THAT(metric_obj["value"].toString().toStdString(), Eq("1"));
            // Ensure 'time' has a valid RFC3339 timestamp
            EXPECT_TRUE(QDateTime::fromString(metric_obj["time"].toString(), Qt::ISODateWithMs).isValid());

            EXPECT_TRUE(metric_obj["tags"].isObject());
            auto tags = metric_obj["tags"].toObject();

            EXPECT_THAT(tags.size(), Eq(1));
            EXPECT_TRUE(tags.contains("multipass_id"));

            // Ensure 'multipass_id' equals the original passed in unique id
            EXPECT_THAT(tags["multipass_id"].toString().toStdString(), Eq(unique_id.toStdString()));
        }
    }
}

TEST_F(MetricsProvider, opt_out_denied_valid)
{
    const auto unique_id = mp::utils::make_uuid();
    mp::MetricsProvider metrics_provider{metrics_file.url(), unique_id, metrics_dir.path()};
    metrics_provider.send_denied();

    wait_for_metrics();

    QFile file{metrics_file.name()};
    file.open(QIODevice::ReadOnly);

    auto data = file.readAll();
    auto json = QJsonDocument::fromJson(data);

    EXPECT_FALSE(json.isNull());
    EXPECT_TRUE(json.isArray());

    auto metric_batches = json.array();

    for (const auto& metric_batch : metric_batches)
    {
        EXPECT_TRUE(metric_batch.isObject());

        auto denied = metric_batch.toObject();
        EXPECT_THAT(denied.size(), Eq(1));
        EXPECT_TRUE(denied.contains("denied"));
        EXPECT_THAT(denied["denied"].toInt(), Eq(1));
    }
}
