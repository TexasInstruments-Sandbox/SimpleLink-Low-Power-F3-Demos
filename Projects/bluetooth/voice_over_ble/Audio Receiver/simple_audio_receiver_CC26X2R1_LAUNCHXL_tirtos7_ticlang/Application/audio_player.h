/*
 * audio_player.h
 *
 *  Created on: Nov 25, 2022
 *      Author: a0132664
 */

#ifndef APPLICATION_AUDIO_PLAYER_H_
#define APPLICATION_AUDIO_PLAYER_H_

extern void AudioPlayer_init(void);
extern void AudioPlayer_addData(uint8_t *rxBitStream, uint16_t bufferLength);

#endif /* APPLICATION_AUDIO_PLAYER_H_ */
