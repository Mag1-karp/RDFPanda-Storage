#ifndef RDFPANDA_STORAGE_MEMORYMONITOR_H
#define RDFPANDA_STORAGE_MEMORYMONITOR_H

#include <iostream>
#include <fstream>
#include <string>
#include <sys/resource.h>

class MemoryMonitor {
public:
    struct MemoryStats {
        size_t currentRSS;      // 当前常驻内存集 (bytes)
        size_t peakRSS;         // 峰值内存使用 (bytes)
        size_t currentVirtual;  // 当前虚拟内存 (bytes)
    };

    static MemoryStats getCurrentMemoryUsage() {
        MemoryStats stats = {0, 0, 0};

        // 使用 /proc/self/status 获取内存信息 (Linux)
        std::ifstream status("/proc/self/status");
        std::string line;

        while (std::getline(status, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                stats.currentRSS = parseMemoryLine(line) * 1024;  // KB to bytes
            } else if (line.substr(0, 6) == "VmPeak") {
                stats.peakRSS = parseMemoryLine(line) * 1024;
            } else if (line.substr(0, 7) == "VmSize:") {
                stats.currentVirtual = parseMemoryLine(line) * 1024;
            }
        }

        return stats;
    }

    static void printMemoryUsage(const std::string& label = "") {
        auto stats = getCurrentMemoryUsage();

        std::cout << "=== Memory Usage";
        if (!label.empty()) {
            std::cout << " [" << label << "]";
        }
        std::cout << " ===" << std::endl;
        std::cout << "  Current RSS:   " << formatBytes(stats.currentRSS) << std::endl;
        std::cout << "  Peak RSS:      " << formatBytes(stats.peakRSS) << std::endl;
        std::cout << "  Virtual Mem:   " << formatBytes(stats.currentVirtual) << std::endl;
    }

private:
    static size_t parseMemoryLine(const std::string& line) {
        size_t pos = line.find_last_of('\t');
        if (pos != std::string::npos) {
            std::string num = line.substr(pos + 1);
            // 移除 " kB" 后缀
            size_t kbPos = num.find(" kB");
            if (kbPos != std::string::npos) {
                num = num.substr(0, kbPos);
            }
            return std::stoull(num);
        }
        return 0;
    }

    static std::string formatBytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unitIndex < 4) {
            size /= 1024.0;
            unitIndex++;
        }

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unitIndex]);
        return std::string(buffer);
    }
};

#endif //RDFPANDA_STORAGE_MEMORYMONITOR_H
