// ui/liuyao_sound.c —— 六爻音效实现（双端共用）。
#include "liuyao_sound.h"
#include "app_port.h"
#include "sound_coin_data.h"

void liuyao_sound_coin(void)
{
    app_port_play_pcm(coin_sound, COIN_SOUND_LEN, COIN_SOUND_RATE);
}
