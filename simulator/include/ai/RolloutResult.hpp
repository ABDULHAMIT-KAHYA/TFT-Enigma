#pragma once

#include <cstdint>

struct RolloutResult
{
    int rollouts = 0;
    float avgEV = 0.0f;
    float avgHP = 0.0f;
    float avgBoard = 0.0f;
    float top4Prob = 0.0f;
    float deathRisk = 0.0f;

    void addSample(float ev, float hp, float board, bool died, float top4);
    void finalize();

private:
    float sumEV = 0.0f;
    float sumHP = 0.0f;
    float sumBoard = 0.0f;
    float sumTop4 = 0.0f;
    int deaths = 0;
};

