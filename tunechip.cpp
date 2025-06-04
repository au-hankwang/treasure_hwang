#include "test.h"

// Comparator function to sort in descending order based on the number
bool compare(const EnginePerf& a, const EnginePerf& b) {
    return a.perf > b.perf;
}

// Must do this early to avoid side effects of updating all engine clocks
int testtype::Remove(PerfExtremes extremes) {
    // set hashconfig.clk_disable
    WriteConfig(E_HASHCONFIG, (1 << 15));
    // Write mask to disable clock to offending engines, okay to have empty vector
    SetMask(extremes.bottom, true);
    // unset hashconfig.clk_disable
    WriteConfig(E_HASHCONFIG, 0);
    return extremes.bottom.size();
}

// if baseline, then enable all engines, and selectively disable some
// if not, then disable all engines, and selectively enable some
void testtype::SetMask(std::vector<EnginePerf> engines, bool isBaseline) {
    for (int i = 0; i < 8; i++) {
        int baseline = 0;
        int val = 1;
        if (engines.size() > 0) {
            for (const auto& e : engines) {
                int regOffset = e.engine / 32;
                int shift = e.engine % 32;
                if (regOffset == i) {
                    baseline |= (val << shift);
                }
            }
            // invert if selecting set B
            if (!isBaseline) {
                baseline = ~baseline;
            }
        }
        WriteConfig(E_ENGINEMASK + i, baseline);
    }
}

float testtype::PartialRunBist(int id, int chips) {
    uint32 bist_reg;
    int loops;
    int iterations = 32;

    //   for (i = 0; i < 8; i++)
    //      WriteConfig(E_ENGINEMASK + i, 0, id);

    WriteConfig(E_BIST_GOOD_SAMPLES, 0, id);
    WriteConfig(E_BIST, iterations, id); // run bist, clearing failures
    for (loops = 0; loops < 100; loops++) {
        bist_reg = ReadConfig(E_BIST, id);
        if (bist_reg == 0) break;
    }
    bist_reg = ReadConfig(E_BIST_GOOD_SAMPLES, id);
    float hitrate = (float)bist_reg / (float)(iterations * 8 * chips);

    // printf("Asic%d bist took %d iterations to complete and hitrate = %.2f%%\n", id, loops, hitrate * 100.0);
    //   for (i = 0; i < 7; i++)
    //      printf("bist results[%d]=%X\n", i, ReadConfig(E_BIST_RESULTS + i, id));
    return hitrate;
}

float testtype::GetVFromBist(int phase, float freq) {
    // enable all engines
    for (int i = 0; i < 8; i++) {
        WriteConfig(E_ENGINEMASK + i, 0);
    }

    // loop through volts
    float hitrate, current;
    for (float volt = 260; volt < 320; volt++) {
        SetVoltage(volt * 0.001); // <<<<<<
        Sleep(1000);
        hitrate = PartialRunBist(-1, 238);

        boardinfotype info2;
        usb.BoardInfo(info2, true);
        current = info2.core_current;
    
        printf("%d %f %f %f %f\n", phase, freq, volt, hitrate, current);
    }

    return 0.00; // we don't need the return value
}

float testtype::GetJthAtV(std::vector<Bucket> bucket, PerfExtremes extremes, float winner_volt, bool ignoreMask) {
    float winner_hr = 0.0;
    float jpth = 0.0; // add efficiency calculation here

    // Calc denominator for buckets. This formula is given by Raju
    for (auto& b : bucket) {
        if (b.n > 0) {

            if (!ignoreMask) {
                SetMask(extremes.top25percent, b.isBaseline);
            }

            // Find Min Volt
            SetVoltage(winner_volt);
            Sleep(1000);
            winner_hr = PartialRunBist(-1, b.n);
            printf("Hitrate is %f at %f\n", winner_hr, winner_volt);

            b.hr = winner_hr;
        }
    }
    // getting current to calculate power
    boardinfotype info2;
    usb.BoardInfo(info2, true);

    float denom = 0.0;
    for (const auto& b : bucket) {
        denom += b.freq * 1e-6 * b.hr * 2 * b.n;
    }
    jpth = winner_volt * info2.core_current / denom;
    printf("denom = %f\n", denom);
    printf("J/TH = %f\n", jpth);
    return jpth;
}


void testtype::MoveToPLL2(std::vector<EnginePerf> engines) {
    for (const auto& e : engines) {
        // Move each engine to 2nd PLL
        WriteConfig(E_MINER_CONFIG, (e.engine << 16) + (1 << 3));
    }
}

void testtype::turnOffBadEngines(std::vector<EnginePerf> badEngines, int maxNum) {
    // jason
    int rows[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int count = 0;
    int curr_row;
    WriteConfig(E_HASHCONFIG, 1 << 15); // 1 << 15 is clock disable
    for (const auto& e : badEngines) {
        curr_row = e.engine / 32;
        rows[curr_row] = rows[curr_row] | 1 << (e.engine % 32);
        if (++count >= maxNum) break;
    }

    for (int i = 0; i < 8; i++) {
        WriteConfig(i + E_ENGINEMASK, rows[i]);
    }

    WriteConfig(E_HASHCONFIG, 1 << 15);
}


PerfExtremes testtype::TestEngineMap(float* map, int phase, int duty_cycle, float topPercent)
{
    TAtype ta("EngineMap");
    const int iterations = 10;
    const float hit = 0.95;
    int i;
    headertype h;
    vector<batchtype> batch_start, batch_end;

    h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
    Load(h, 64, 0, -1);
    usb.OLED_Display("EngineMap\n#\n@");

    Pll(100);
    WriteConfig(E_VERSION_BOUND, 0xff0000);
    for (i = 0; i < 8; i++)
        WriteConfig(E_ENGINEMASK + i, 0xffffffff);
    const int num_engines = 1;
    WriteConfig(E_HASHCONFIG, phase | (1 << 15) | (1 << 16));
    WriteConfig(E_HASHCONFIG, phase | (1 << 15));
    WriteConfig(E_BIST, 1);

    WriteConfig(E_SPEED_INCREMENT, pll_multiplier * 5);
    WriteConfig(E_SPEED_DELAY, 1000);
    WriteConfig(E_SPEED_UPPER_BOUND, pll_multiplier * 3000);
    WriteConfig(E_BIST_THRESHOLD, (iterations - 1) * num_engines * 8 * hit);
    if (duty_cycle == 0)
        WriteConfig(E_DUTY_CYCLE, 0);
    else if (duty_cycle < 0)
        WriteConfig(E_DUTY_CYCLE, 0x8000 | (-duty_cycle + 0x80));
    else
        WriteConfig(E_DUTY_CYCLE, 0x80 | ((duty_cycle + 0x80) << 8));
    WriteConfig(E_HASHCONFIG, (1 << 15) | phase | (1 << 16));
    WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
    WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
    WriteConfig(E_BIST, 1);

    float average = 0.0;
    std::vector<EnginePerf> maxSpeed(238);

    for (i = 0; i < 238; i++)
    {
        WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff ^ (1 << (i & 31)));

        WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
        WriteConfig(E_BIST_GOOD_SAMPLES, 0);
        WriteConfig(E_PLLFREQ, 100.0 * pll_multiplier);
        WriteConfig(E_BIST, iterations + (1 << 31));
        while (ReadConfig(E_BIST) != 0)
            ;
        float f = ReadConfig(E_PLLFREQ) / pll_multiplier;
        map[i] = f;
        average += f;
        maxSpeed[i] = { i, f };

        WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff);
    }
    average = average / 238;
    // Sort the results
    std::sort(maxSpeed.begin(), maxSpeed.end(), compare);
    // return the top 25%
    std::vector<EnginePerf> top25percent(maxSpeed.begin(), maxSpeed.begin() + (238 * topPercent));

    printf("Engine map for hit %.2f phase %d duty=%d temp=%.1fC, Average f=%.0f\n", hit, phase, duty_cycle, OnDieTemp(), average);
    Pll(25, -1);

    for (int row = 0; row < 19; row++)
    {
        for (i = 11; i >= 6; i--) {
            printf(" %4.0f", map[row * 12 + i]);
        }
        printf(" ");
        for (i = 0; i <= 5; i++) {
            printf(" %4.0f", map[row * 12 + i]);
        }
        printf("\n");
    }
    for (i = 9; i >= 5; i--) {
        printf(" %4.0f", map[19 * 12 + i]);
    }
    printf("           ");
    for (i = 0; i <= 4; i++) {
        printf(" %4.0f", map[19 * 12 + i]);
    }
    printf("\n");
    printf("The top %d engines are:\n", top25percent.size());
    for (const auto& e : top25percent) {
        printf("Engine %d, @%fMHz\n", e.engine, e.perf);
    }
    printf("Critical freq: %f\n", top25percent[top25percent.size() - 1].perf);

    i = 0;
    // reverse search sorted perf list for dogs
    reverse(maxSpeed.begin(), maxSpeed.end());
    for (const auto& e : maxSpeed) {
        if (i > 5)
            break;
        if (e.perf > 800)
            break;
        i++;
    }

    // generate a vector of very slow engines
    std::vector<EnginePerf> bottom(maxSpeed.begin(), maxSpeed.begin() + i);
    printf("The bottom %d engines are:\n", bottom.size());
    for (const auto& e : bottom) {
        printf("Engine %d, @%fMHz\n", e.engine, e.perf);
    }


    return PerfExtremes{ maxSpeed, top25percent, bottom, 238 - i };
}
