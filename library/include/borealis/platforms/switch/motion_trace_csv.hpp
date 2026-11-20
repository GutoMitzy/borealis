#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace brls {

inline std::uint64_t motionTraceMicros()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

class MotionTraceCsv {
public:
    MotionTraceCsv(const char* enablePath, const char* csvPath,
                   const char* columns, unsigned maxRows = 100000)
        : enablePath(enablePath), csvPath(csvPath),
          columns(columns), maxRows(maxRows)
    {
    }

    ~MotionTraceCsv()
    {
        if (file) {
            std::fflush(file);
            std::fclose(file);
        }
    }

    MotionTraceCsv(const MotionTraceCsv&) = delete;
    MotionTraceCsv& operator=(const MotionTraceCsv&) = delete;

    bool enabled()
    {
        if (!initialized) {
            initialized = true;
            if (FILE* flag = std::fopen(enablePath, "r")) {
                std::fclose(flag);
                file = std::fopen(csvPath, "w");
                if (file) {
                    std::setvbuf(file, buffer, _IOFBF, sizeof(buffer));
                    std::fprintf(file, "%s\n", columns);
                }
            }
        }
        return file && rows < maxRows;
    }

    void write(const char* format, ...)
    {
        if (!enabled())
            return;

        va_list args;
        va_start(args, format);
        std::vfprintf(file, format, args);
        va_end(args);
        std::fputc('\n', file);
        ++rows;
        if ((rows % 128U) == 0U || rows == maxRows)
            std::fflush(file);
    }

    unsigned rowCount() const { return rows; }

private:
    const char* enablePath;
    const char* csvPath;
    const char* columns;
    const unsigned maxRows;
    bool initialized = false;
    FILE* file = nullptr;
    unsigned rows = 0;
    char buffer[65536] = {};
};

}
