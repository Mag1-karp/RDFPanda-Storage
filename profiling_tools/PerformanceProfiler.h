#ifndef RDFPANDA_STORAGE_PERFORMANCEPROFILER_H
#define RDFPANDA_STORAGE_PERFORMANCEPROFILER_H

#include <chrono>
#include <iostream>
#include <string>
#include <map>
#include <iomanip>

class PerformanceProfiler {
private:
    struct Metric {
        std::chrono::duration<double> totalTime;
        size_t callCount;
        std::chrono::duration<double> minTime;
        std::chrono::duration<double> maxTime;

        Metric() : totalTime(0), callCount(0),
                   minTime(std::chrono::duration<double>::max()),
                   maxTime(std::chrono::duration<double>::min()) {}
    };

    std::map<std::string, Metric> metrics;

public:
    class Timer {
    private:
        std::chrono::high_resolution_clock::time_point start;
        std::string name;
        PerformanceProfiler* profiler;

    public:
        Timer(const std::string& n, PerformanceProfiler* p)
            : name(n), profiler(p) {
            start = std::chrono::high_resolution_clock::now();
        }

        ~Timer() {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = end - start;
            profiler->recordMetric(name, elapsed);
        }
    };

    void recordMetric(const std::string& name, std::chrono::duration<double> elapsed) {
        auto& metric = metrics[name];
        metric.totalTime += elapsed;
        metric.callCount++;
        metric.minTime = std::min(metric.minTime, elapsed);
        metric.maxTime = std::max(metric.maxTime, elapsed);
    }

    void printReport() const {
        std::cout << "\n=== Performance Profile Report ===" << std::endl;
        std::cout << std::setw(40) << std::left << "Operation"
                  << std::setw(12) << std::right << "Calls"
                  << std::setw(15) << "Total(s)"
                  << std::setw(15) << "Avg(s)"
                  << std::setw(15) << "Min(s)"
                  << std::setw(15) << "Max(s)" << std::endl;
        std::cout << std::string(112, '-') << std::endl;

        for (const auto& [name, metric] : metrics) {
            double avg = metric.totalTime.count() / metric.callCount;
            std::cout << std::setw(40) << std::left << name
                      << std::setw(12) << std::right << metric.callCount
                      << std::setw(15) << std::fixed << std::setprecision(6) << metric.totalTime.count()
                      << std::setw(15) << avg
                      << std::setw(15) << metric.minTime.count()
                      << std::setw(15) << metric.maxTime.count() << std::endl;
        }
        std::cout << std::string(112, '=') << std::endl;
    }

    void reset() {
        metrics.clear();
    }
};

// 单例模式
class GlobalProfiler {
public:
    static PerformanceProfiler& getInstance() {
        static PerformanceProfiler instance;
        return instance;
    }
};

// 便捷宏
#define PROFILE_SCOPE(name) PerformanceProfiler::Timer __timer##__LINE__(name, &GlobalProfiler::getInstance())
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

#endif //RDFPANDA_STORAGE_PERFORMANCEPROFILER_H
