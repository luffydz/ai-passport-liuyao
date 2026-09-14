// fw/main/app_port_impl.h —— 真机侧平台实现的额外入口（不属于双端共用接口）。
#pragma once

// 启动音频 worker 任务。必须在 bsp_audio_init() 成功之后调用。
// 之后 app_port_play_pcm() 只是往队列里投递，不会阻塞按键任务。
void app_port_impl_audio_start(void);
