#include <borealis/platforms/switch/motion_trace_csv.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>

int main()
{
    const char* flag = "/tmp/joycon_trace_flag_v15";
    const char* output = "/tmp/joycon_trace_rows_v15.csv";
    std::remove(flag);
    std::remove(output);
    {
        brls::MotionTraceCsv disabled(flag, output, "a,b");
        assert(!disabled.enabled());
        disabled.write("%d,%d", 1, 2);
        assert(disabled.rowCount() == 0);
    }
    {
        FILE* f = std::fopen(flag, "w");
        assert(f);
        std::fclose(f);
    }
    {
        brls::MotionTraceCsv active(flag, output, "time,slot,val", 2);
        assert(active.enabled());
        assert(brls::motionTraceMicros() != 0);
        active.write("%d,%d,%d", 1, 2, 3);
        active.write("%d,%d,%d", 4, 5, 6);
        active.write("%d,%d,%d", 7, 8, 9);
        assert(active.rowCount() == 2);
        assert(!active.enabled());
    }
    {
        FILE* f = std::fopen(output, "r");
        assert(f);
        char line[256] = {};
        assert(std::fgets(line, sizeof(line), f));
        assert(std::strcmp(line, "time,slot,val\n") == 0);
        assert(std::fgets(line, sizeof(line), f));
        assert(std::strcmp(line, "1,2,3\n") == 0);
        assert(std::fgets(line, sizeof(line), f));
        assert(std::strcmp(line, "4,5,6\n") == 0);
        assert(!std::fgets(line, sizeof(line), f));
        std::fclose(f);
    }
    std::remove(flag);
    std::remove(output);
}
