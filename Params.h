#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace Px
{
enum { gain, hpf, lpf, ptune, pmix, dmode, drive, dtone, dmix, bits, down, cmix,
       rfreq, ringmix, gdiv, gdepth, gshape, fcut, freso, fmode, flfo, frate, fmix,
       dlmix, dldiv, dlfb, dldirt, rvmix, rvsize, rvdamp, dbl, destroy, outg, mix, NP };
}

struct Spec
{
    const char* id; const char* name; const char* label;
    float lo, hi, def, step, skewMid;
    const char* choices;   // "a|b|c" dla parametrow wyboru, inaczej nullptr
    const char* unit;
};

inline const Spec specs[Px::NP] = {
    { "gain",   "Input Gain",        "GAIN",     -12, 12, 0,   0.1f, 0,    nullptr, " dB" },
    { "hpf",    "High-Pass",         "HPF",       20, 400, 80, 1,    150,  nullptr, " Hz" },
    { "lpf",    "Low-Pass",          "LPF",     1000, 20000, 18000, 1, 6000, nullptr, " Hz" },
    { "ptune",  "Pitch Tune",        "TUNE",     -24, 24, 0,   0.1f, 0,    nullptr, " st" },
    { "pmix",   "Pitch Layer",       "LAYER",      0, 100, 0,  1,    0,    nullptr, " %" },
    { "dmode",  "Dist Mode",         "MODE",       0, 3, 0,    1,    0,    "Soft|Hard|Fold|Fuzz", nullptr },
    { "drive",  "Dist Drive",        "DRIVE",      0, 100, 0,  1,    0,    nullptr, " %" },
    { "dtone",  "Dist Tone",         "TONE",     500, 16000, 12000, 1, 4000, nullptr, " Hz" },
    { "dmix",   "Dist Mix",          "MIX",        0, 100, 0,  1,    0,    nullptr, " %" },
    { "bits",   "Crush Bits",        "BITS",       2, 16, 16,  0.1f, 0,    nullptr, " bit" },
    { "down",   "Crush Rate",        "DOWN",       1, 40, 1,   1,    0,    nullptr, "x" },
    { "cmix",   "Crush Mix",         "MIX",        0, 100, 0,  1,    0,    nullptr, " %" },
    { "rfreq",  "Ring Freq",         "FREQ",      20, 3000, 220, 1,  300,  nullptr, " Hz" },
    { "ringmix","Ring Mix",          "MIX",        0, 100, 0,  1,    0,    nullptr, " %" },
    { "gdiv",   "Gate Rate",         "RATE",       0, 3, 1,    1,    0,    "1/4|1/8|1/16|1/32", nullptr },
    { "gdepth", "Gate Depth",        "DEPTH",      0, 100, 0,  1,    0,    nullptr, " %" },
    { "gshape", "Gate Chaos",        "CHAOS",      0, 100, 0,  1,    0,    nullptr, " %" },
    { "fcut",   "Filter Cutoff",     "CUTOFF",    80, 12000, 2000, 1, 1000, nullptr, " Hz" },
    { "freso",  "Filter Reso",       "RESO",       0, 100, 20, 1,    0,    nullptr, " %" },
    { "fmode",  "Filter Type",       "TYPE",       0, 2, 0,    1,    0,    "Low|Band|High", nullptr },
    { "flfo",   "Filter Sweep",      "SWEEP",      0, 100, 0,  1,    0,    nullptr, " %" },
    { "frate",  "Filter Speed",      "SPEED",      0, 4, 2,    1,    0,    "1 bar|1/2|1/4|1/8|1/16", nullptr },
    { "fmix",   "Filter Mix",        "MIX",        0, 100, 0,  1,    0,    nullptr, " %" },
    { "dlmix",  "Delay Mix",         "MIX",        0, 100, 0,  1,    0,    nullptr, " %" },
    { "dldiv",  "Delay Time",        "TIME",       0, 4, 1,    1,    0,    "1/16|1/8|1/8d|1/4|1/2", nullptr },
    { "dlfb",   "Delay Feedback",    "FEEDBACK",   0, 100, 30, 1,    0,    nullptr, " %" },
    { "dldirt", "Delay Dirt",        "DIRT",       0, 100, 0,  1,    0,    nullptr, " %" },
    { "rvmix",  "Reverb Mix",        "MIX",        0, 100, 0,  1,    0,    nullptr, " %" },
    { "rvsize", "Reverb Size",       "SIZE",       0, 100, 50, 1,    0,    nullptr, " %" },
    { "rvdamp", "Reverb Damping",    "DAMP",       0, 100, 50, 1,    0,    nullptr, " %" },
    { "dbl",    "Doubler Width",     "WIDTH",      0, 100, 0,  1,    0,    nullptr, " %" },
    { "destroy","DESTROY",           "DESTROY",    0, 100, 0,  1,    0,    nullptr, " %" },
    { "outg",   "Output Gain",       "OUT",      -12, 12, 0,   0.1f, 0,    nullptr, " dB" },
    { "mix",    "Dry/Wet",           "DRY/WET",    0, 100, 100, 1,   0,    nullptr, " %" },
};

inline const char* presetNames =
    "Custom|Clean Cut|Crypt Whisper|Dungeon Choir|Blood Static|Cathedral Void|"
    "Glitch Reaper|Demon Octave|Rust & Ash|Total Noise";

// kolejnosc jak w enum Px:
// gain,hpf,lpf | tune,layer | mode,drive,tone,mix | bits,down,mix | freq,mix | rate,depth,chaos |
// cut,reso,type,sweep,speed,mix | dmix,time,fb,dirt | rvmix,size,damp | width | destroy,out,drywet
inline const float presets[9][Px::NP] = {
  { 0,90,18000,   0,0,    0,10,14000,20,   16,1,0,   220,0,   1,0,0,    2000,20,0,0,2,0,    0,1,25,0,    8,40,50,    10,   0,0,100 },
  { 0,120,9000,  -12,25,  0,20,6000,35,    12,1,10,  220,0,   1,0,0,    1800,25,0,15,0,30,  18,2,45,20,  45,85,60,   30,   5,-1,100 },
  { 0,110,16000, -12,45,  0,15,9000,25,    16,1,0,   220,0,   1,0,0,    3000,20,0,0,2,0,    20,3,40,10,  40,90,40,   60,   0,-1,100 },
  { 2,150,14000,  0,0,    3,55,7000,70,    8,3,35,   440,18,  1,0,0,    2500,35,1,25,3,40,  8,0,25,30,   12,45,70,   20,   30,-2,100 },
  { 0,130,12000, 12,30,   0,10,8000,20,    16,1,0,   220,0,   1,0,0,    1500,30,0,10,0,25,  25,3,60,15,  70,100,35,  50,   0,-2,100 },
  { 0,140,16000,  0,0,    1,40,10000,50,   6,6,25,   150,10,  2,70,60,  3000,45,1,40,4,50,  15,0,40,25,  10,35,60,   15,   20,-2,100 },
  { 2,100,12000,-12,70,   3,35,5500,50,    10,2,10,  110,12,  1,10,10,  1200,30,0,20,1,30,  10,2,35,20,  25,70,60,   40,   15,-2,100 },
  { 0,200,6500,   0,0,    0,30,5000,60,    7,4,40,   220,0,   1,0,0,    2200,15,1,0,2,25,   12,3,35,35,  15,50,80,   10,   10,-1,100 },
  { 3,180,16000, -7,40,   2,90,9000,100,   4,10,60,  880,35,  3,95,100, 4000,70,1,70,4,70,  30,0,70,60,  25,85,60,   80,   85,-4,100 },
};
