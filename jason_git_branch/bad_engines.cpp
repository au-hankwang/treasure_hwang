bool testtype::TestPoint(float temp, const char* partname, int station, gpiotype& gpio, bool abbreviated, bool turbo) {
    // jason
    bool reject = false;
    boardinfotype info;
    datatype data;

    time(&data.seconds);
    if (strlen(partname) >= sizeof(data.partname)) FATAL_ERROR;
    strcpy(data.partname, partname);
    data.station = station;

    printf("Testpoint for %s: %.0fC\n", data.partname, temp);
    usb.SetVoltage(S_SCV, 750);
    usb.SetVoltage(S_IO, 1500);
    usb.SetVoltage(S_CORE, 290);
    ControlTest(data);

    usb.SetVoltage(S_CORE, 290);
    Pll(0, -1, false);
    Pll(0, -1, true);
    Delay(100);

    usb.BoardInfo(info, true);
    data.temp = temp;
    data.iddq_temp = OnDieTemp();
    float odv = OnDieVoltage();
    data.iddq = info.core_current;
    float iddq_normalized = data.iddq * exp2((85 - data.iddq_temp) / 25.0);
    printf("Voltage = %.1fmV Temp=%.1fC IDDQ=%.2fA Normalized to 85C=%.2fA\n", odv * 1000.0, data.iddq_temp, data.iddq, iddq_normalized);

    SetVoltage(0);
    SetVoltage(200);
    char sfilename[256];
    sprintf(sfilename, "shmoo\\%s_enginemap.csv", partname);

    SetVoltage(0);
    SetVoltage(.290);
    std::vector<int> badEngines = EngineMapHelper(sfilename, data.engine_map, 0);

    int hashcfg = ReadConfig(E_HASHCONFIG);
    printf("hashcfg: %x\n", hashcfg);
    for (int i = 0; i < 8; i++) {
        WriteConfig(E_ENGINEMASK + i, 0);
    }

    // Set starting frequency
    printf("duty cycle %x\n", ReadConfig(E_DUTY_CYCLE));
    Pll(1100, -1, false, 5);

    float winner;
    for (float volt = 0.300; volt > 0.250; volt -= 0.001) {
        SetVoltage(volt);
        Sleep(1000);
        float res = RunBist(-1);
        printf("Hitrate is %f at %f\n", res, volt);
        winner = volt;
        if (res < 0.97) {
            break;
        }
    }
    float jpth; // add efficiency calculation here
    boardinfotype info2;
    usb.BoardInfo(info2, true);
    jpth = winner * info2.core_current / (1.1 / 1000 * 0.98 * 2 * 238);
    printf("J/TH = %f\n", jpth);

    Pll(800, -1, true, 5); // config pll2
    Sleep(1000);

    configBadEngines(badEngines); // switch bad engines to pll2

    for (float volt = 0.300; volt > 0.250; volt -= 0.001) {
        SetVoltage(volt);
        Sleep(1000);
        float res = RunBist(-1);
        winner = volt;
        printf("Hitrate is %f at %f\n", res, volt);
        if (res < 0.97) {
            break;
        }
    }
    usb.BoardInfo(info2, true);
    jpth = winner * info2.core_current / (1.1 / 1000 * 0.98 * 2 * 238);
    printf("J/TH = %f\n", jpth);

    return reject;
}




std::vector<int> testtype::EngineMapHelper(const char *filename, float *map, int phase, int duty_cycle) {
    // jason
    TAtype ta("EngineMapHelper");
    const int iterations = 10;
    const float hit = 0.95;
    int i;
    headertype h;
    vector<batchtype> batch_start, batch_end;

    h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
    Load(h, 64, 0, -1);
    usb.OLED_Display("EngineMapHelper\n#\n@");

    Pll(100);
    WriteConfig(E_VERSION_BOUND, 0xff0000);
    for (i = 0; i < 8; i++)
        WriteConfig(E_ENGINEMASK + i, 0xffffffff);
    const int num_engines = 1;
    WriteConfig(E_HASHCONFIG, phase | (1 << 15)| (1 << 16));
    WriteConfig(E_HASHCONFIG, phase| (1 << 15));
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

    float average=0.0;
    // printf("BAD = \n");

    std::vector<int> badEngines;
    for (i = 0; i < 238; i++) {
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
        if (f < 200) {
            // printf("%d, ", i);
            badEngines.push_back(i);
        }
        WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff);
    }
    average = average / 238;

    printf("Engine map for hit %.2f phase %d duty=%d temp=%.1fC, Average f=%.0f\n", hit, phase, duty_cycle,OnDieTemp(), average);
    Pll(25, -1);

    if (filename == NULL) return;
    FILE* fptr = fopen(filename, "at");
    if (fptr == NULL) { printf("I couldn't open %s for append\n",filename); return; }
    fprintf(fptr, "Engine map for hit%.2f phase %d duty=%d temp=%.1fC Average f=%.2f\n", hit, phase, duty_cycle, OnDieTemp(), average);
    for (int row = 0; row < 19; row++) {
        for (i = 11; i >= 6; i--) {
            fprintf(fptr, ",%.0f", map[row * 12 + i]);
            printf(" %4.0f", map[row * 12 + i]);
        }
        fprintf(fptr, ",");
        printf(" ");
        for (i = 0; i <= 5; i++) {
            fprintf(fptr, ",%.0f", map[row * 12 + i]);
            printf(" %4.0f", map[row * 12 + i]);
        }
        fprintf(fptr, "\n");
        printf("\n");
    }
    for (i = 9; i >= 5; i--) {
        fprintf(fptr, ",%.0f", map[19 * 12 + i]);
        printf(" %4.0f", map[19 * 12 + i]);
    }
    fprintf(fptr, ",,,");
    printf("           ");
    for (i = 0; i <= 4; i++) {
        fprintf(fptr, ",%.0f", map[19 * 12 + i]);
        printf(" %4.0f", map[19 * 12 + i]);
    }
    fprintf(fptr, "\n");
    printf("\n");
    fclose(fptr);
    return badEngines;
}


void testtype::turnOffEngines(std::vector<int> badEngines) {
    // jason
    WriteConfig(E_HASHCONFIG, 1 << 15); // 1 << 15 is clock disable
    for (int engineNo : badEngines) {
        WriteConfig(engineNo / 32 + E_ENGINEMASK, 1 << (engineNo % 32));
    }
    WriteConfig(E_HASHCONFIG, 0 << 15);
}
