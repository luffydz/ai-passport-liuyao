// sim/host/sim_audio.c —— app_port_play_pcm() 在【模拟器侧】的实现（SDL 音频）。
//
// SDL_QueueAudio 本身就是异步队列，天然满足"非阻塞"约定 ——
// 真机侧要自己用 worker 任务达到同样的效果（契约写在 ui/app_port.h）。
#include "sim_audio.h"
#include "app_port.h"

#include <SDL2/SDL.h>
#include <stdio.h>

static SDL_AudioDeviceID s_dev;
static int s_dev_rate;

void sim_audio_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "  [audio] SDL 音频初始化失败: %s（音效将静音）\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq     = 22050;          // 与 coin_sound 数据一致
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 1024;

    s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!s_dev) {
        fprintf(stderr, "  [audio] 打开音频设备失败: %s（音效将静音）\n", SDL_GetError());
        return;
    }
    s_dev_rate = have.freq;
    SDL_PauseAudioDevice(s_dev, 0);   // 立即开始消费队列
    printf("  [audio] SDL 音频就绪：%d Hz mono s16\n", s_dev_rate);
}

void app_port_play_pcm(const int16_t *samples, int count, int sample_rate)
{
    if (!s_dev || !samples || count <= 0) return;   // 出图模式没开音频，静默跳过

    if (sample_rate != s_dev_rate) {
        // 不做重采样，直接明确告警，避免"以为在放其实没放"
        fprintf(stderr, "  [audio] 采样率不匹配（数据 %d / 设备 %d），跳过播放\n",
                sample_rate, s_dev_rate);
        return;
    }
    SDL_QueueAudio(s_dev, samples, (Uint32)count * 2);
}
