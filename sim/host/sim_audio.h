// sim/host/sim_audio.h —— 模拟器侧的音频后端（SDL）。
#pragma once

// 在 SDL_Init 之后调用；失败只告警，不影响仿真继续跑
void sim_audio_init(void);
