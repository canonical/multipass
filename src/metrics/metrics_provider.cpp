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

#include <multipass/logging/log.h>
#include <multipass/metrics/metrics_histogram.h>
#include <multipass/metrics/metrics_provider.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace mp = multipass;
namespace mpl = multipass::logging;

using namespace std::literals::chrono_literals;

namespace
{
constexpr auto category = "metrics";
constexpr auto saved_metrics_file = "saved_metrics.json";

mp::HistogramBins memory_bins{{512, 1024, 2048, 4096, 8196},
                              {"less_than_512MB", "512MB_to_1023MB", "1024MB_to_2047MB", "2048MB_to_4095MB",
                               "4096MB_to_8195MB", "8196MB_and_greater"}};

mp::HistogramBins disk_bins{{4, 8, 16}, {"less_than_4GB", "4GB_to_7GB", "8GB_to_15GB", "16GB_and_greater"}};

mp::HistogramBins mount_bins{{1, 2, 4, 8, 16, 32, 64},
                             {"0_mounts", "1_mount", "2_to_3_mounts", "4_to_7_mounts", "8_to_15_mounts",
                              "16_to_31_mounts", "32_to_63_mounts", "64_and_greater_mounts"}};

mp::HistogramBins cpu_bins{{2, 4, 6}, {"1_cpu", "2_to_3_cpus", "4_to_5_cpus", "6_and_greater_cpus"}};

void post_request(const QUrl& metrics_url, const QByteArray& body)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{metrics_url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
    request.setHeader(QNetworkRequest::UserAgentHeader, "multipassd/1.0");

    QEventLoop event_loop;
    std::unique_ptr<QNetworkReply> reply{metrics_url.isLocalFile() ? manager.put(request, body)
                                                                   : manager.post(request, body)};

    QObject::connect(reply.get(), &QNetworkReply::finished, &event_loop, &QEventLoop::quit);

    event_loop.exec();

    auto buff = reply->readAll();

    if (reply->error() == QNetworkReply::ProtocolInvalidOperationError)
    {
        auto error_msg = QJsonDocument::fromJson(buff).object();
        mpl::log(mpl::Level::error, category,
                 fmt::format("Metrics error: {} - {}", error_msg["code"].toString().toStdString(),
                             error_msg["message"].toString().toStdString()));
    }
    else if (reply->error() != QNetworkReply::NoError)
    {
        QObject::disconnect(reply.get(), 0, 0, 0);
        throw std::runtime_error(fmt::format("Metrics error: {}", reply->errorString().toStdString()));
    }
}

auto load_saved_metrics(const mp::Path& data_path)
{
    QFile metrics_file{QDir(data_path).filePath(saved_metrics_file)};

    if (!metrics_file.exists())
        return QJsonArray();

    metrics_file.open(QIODevice::ReadOnly);
    auto metrics = QJsonDocument::fromJson(metrics_file.readAll()).array();

    return metrics;
}

void persist_metrics(const QByteArray& metrics, const mp::Path& data_path)
{
    QFile metrics_file{QDir(data_path).filePath(saved_metrics_file)};
    metrics_file.open(QIODevice::WriteOnly);
    metrics_file.write(metrics);
}
} // namespace

mp::MetricsProvider::MetricsProvider(const QUrl& url, const QString& unique_id, const mp::Path& path)
    : metrics_url{url},
      unique_id{unique_id},
      data_path{path},
      metric_batches(load_saved_metrics(data_path)),
      metrics_available{!metric_batches.isEmpty()},
      metrics_sender{[this] {
          std::unique_lock<std::mutex> lock(metrics_mutex);
          auto timeout = std::chrono::seconds(3600);
          auto metrics_failed{false};

          while (running)
          {
              metrics_cv.wait_for(lock, timeout, [&] { return metrics_available || !running; });

              if (!running)
                  return;

              if (!metrics_available && !metrics_failed)
                  continue;

              auto saved_metrics(metric_batches);
              auto body = QJsonDocument(metric_batches).toJson(QJsonDocument::Compact);
              lock.unlock();

              try
              {
                  post_request(metrics_url, body);

                  if (metrics_failed)
                      metrics_failed = false;

                  lock.lock();

                  if (metric_batches == saved_metrics)
                  {
                      metric_batches = QJsonArray{};
                      metrics_available = false;

                      if (timeout != std::chrono::seconds(3600))
                      {
                          timeout = std::chrono::seconds(3600);
                      }
                  }
                  else
                  {
                      for (auto i = 0; i < saved_metrics.size(); ++i)
                      {
                          metric_batches.removeFirst();
                      }

                      timeout = std::chrono::seconds::zero();
                  }

                  persist_metrics(QJsonDocument(metric_batches).toJson(QJsonDocument::Compact), data_path);
              }
              catch (const std::exception& e)
              {
                  mpl::log(mpl::Level::error, category, fmt::format("{} - Attempting to resend", e.what()));

                  metrics_failed = true;

                  if (timeout == std::chrono::seconds(3600))
                  {
                      timeout = std::chrono::seconds(30);
                  }
                  else if (timeout < std::chrono::minutes(30))
                  {
                      timeout *= 2;
                  }
                  lock.lock();

                  if (metrics_available)
                      metrics_available = false;
              }
          }
      }}
{
}

mp::MetricsProvider::MetricsProvider(const QString& metrics_url, const QString& unique_id, const mp::Path& path)
    : MetricsProvider{QUrl{metrics_url}, unique_id, path}
{
}

mp::MetricsProvider::~MetricsProvider()
{
    {
        std::lock_guard<std::mutex> lck(metrics_mutex);
        running = false;
    }
    metrics_cv.notify_one();
}

bool mp::MetricsProvider::send_metrics(const MetricsData& metrics_data)
{
    QJsonArray metrics;
    MetricsHistogram instance_mem_histogram{memory_bins.bins};
    MetricsHistogram instance_disk_histogram{disk_bins.bins};
    MetricsHistogram instance_mounts_histogram{mount_bins.bins};
    MetricsHistogram instance_cpus_histogram{cpu_bins.bins};

    QJsonObject tags;
    tags.insert("multipass_id", unique_id);

    for (const auto& instance : metrics_data.instances)
    {
        instance_mem_histogram.record(instance.mem_size);
        instance_disk_histogram.record(instance.disk_size);
        instance_mounts_histogram.record(instance.num_mounts);
        instance_cpus_histogram.record(instance.num_cpus);
    }

    auto add_histogram_metrics = [&metrics, &tags](const MetricsHistogram& histogram,
                                                   const std::vector<std::string> bin_strings) {
        auto bin{0};
        for (const auto& bin_string : bin_strings)
        {
            QJsonObject metric;
            metric.insert("key", QString::fromStdString(fmt::format("instances_with_{}", bin_string)));
            metric.insert("value", histogram.count(bin++));
            metric.insert("time", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
            metric.insert("tags", tags);

            metrics.push_back(metric);
        }
    };

    add_histogram_metrics(instance_mem_histogram, memory_bins.bin_strings);
    add_histogram_metrics(instance_disk_histogram, disk_bins.bin_strings);
    add_histogram_metrics(instance_mounts_histogram, mount_bins.bin_strings);
    add_histogram_metrics(instance_cpus_histogram, cpu_bins.bin_strings);

    QJsonObject metric_batch;
    metric_batch.insert("uuid", mp::utils::make_uuid());
    metric_batch.insert("created", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    metric_batch.insert("metrics", metrics);
    metric_batch.insert("credentials", QString());

    update_and_notify_sender(metric_batch);

    return true;
}

void mp::MetricsProvider::send_denied()
{
    QJsonObject denied;
    denied.insert("denied", 1);

    update_and_notify_sender(denied);
}

void mp::MetricsProvider::update_and_notify_sender(const QJsonObject& metric)
{
    {
        std::lock_guard<std::mutex> lck(metrics_mutex);
        metric_batches.push_back(metric);
        persist_metrics(QJsonDocument(metric_batches).toJson(QJsonDocument::Compact), data_path);
        metrics_available = true;
    }
    metrics_cv.notify_one();
}
