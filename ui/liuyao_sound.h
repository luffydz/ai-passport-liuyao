// ui/liuyao_sound.h —— 六爻音效。
//
// 数据在 ui/sound_coin_data.h（由原项目移植，22050Hz/16bit/mono/57.6KB）。
// 播放一律走 app_port_play_pcm() 的非阻塞接口 —— 真机侧要丢给音频 worker，
// 不能在按键回调里直接写 I2S（会卡住按键任务）。
#pragma once

// 摇卦：铜钱翻转声（1.3 秒）
void liuyao_sound_coin(void);
