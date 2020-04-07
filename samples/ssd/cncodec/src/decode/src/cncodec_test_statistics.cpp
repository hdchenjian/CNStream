#include "cncodec_test_statistics.h"
#include "cncodec_test_common.h"
#include <cassert>
#include <algorithm>

using namespace std;

void CnCodecPerfMonitor::add_record(u64_t fine_latency_us,
                                    u64_t raw_latency_us) noexcept
{
    tot_ += fine_latency_us;
    count_++;
    min_=std::min(min_, raw_latency_us);
    max_=std::max(max_, raw_latency_us);
    LOG_TRACE("update tot latency (%lu us), cnt %u, fine(%lu), raw(%lu), pipe gain (%lu)\n",
         tot_, count_, fine_latency_us, raw_latency_us, (raw_latency_us - fine_latency_us));
}

void CnCodecPerfMonitor::print(const std::string& prefix) const noexcept
{
   LOG_INFO("average latency: %lf ms\n", average_latency_ms());
   LOG_INFO("total latency:   %lf ms\n", (double)tot_/1000);
   LOG_INFO("maximum latency: %lf ms\n", (double)max_/1000);
   LOG_INFO("minmum latency: %lf ms\n", (double)min_/1000);
   LOG_INFO("total latency record count(%u)\n", count_);
}

void CnCodecTestRawPerfMonitor::issue(void) noexcept
{
   std::lock_guard<std::mutex> lock(mutex_);
   if (!one_shot_done_)
   {
      one_shot_done_=true;
      start_ = std::chrono::system_clock::now();
      LOG_TRACE("perf mointor issue time %lu\n", CnCodecTestTime::pts());
   }
}

void CnCodecTestRawPerfMonitor::probe(void) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = std::chrono::system_clock::now();
    u64_t latency = CnCodecTestTime::dur_us(start_, stop_);
    add_record(latency, latency);
    start_ = std::chrono::system_clock::now();
}

void CnCodecTestFinePerfMonitor::issue(u64_t key) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    time_point now = std::chrono::system_clock::now();

    map_[key].issued_time     = now;
    map_[key].pipe_start_time = now;
}

void CnCodecTestFinePerfMonitor::probe(u64_t key) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    StartTimePair start;

    try
    {
        start = map_.at(key);
    }
    catch (const std::out_of_range&)
    {
        LOG_ERROR("no start record for key%lu\n", key);
    }

    map_.erase(key);

    time_point now = std::chrono::system_clock::now();
    assert (start.issued_time < now);

    u64_t fine_latency = CnCodecTestTime::dur_us(start.pipe_start_time, now);
    u64_t raw_latency  = CnCodecTestTime::dur_us(start.issued_time, now);

    add_record(fine_latency, raw_latency);

    for (auto iter = map_.begin(); iter != map_.end(); ++iter)
    {
        if (iter->second.pipe_start_time < now)
        {
            iter->second.pipe_start_time = now;
        }
    }
}
