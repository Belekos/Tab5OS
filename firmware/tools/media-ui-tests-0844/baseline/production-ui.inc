static void setLabelIfChanged(lv_obj_t* label, const char* text) {
  if (label && text && strcmp(lv_label_get_text(label), text) != 0)
    lv_label_set_text(label, text);
}
static void setTopActionVisible(lv_obj_t* obj, bool visible) {
  if (!obj) return;
  if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
static void setButtonEnabled(lv_obj_t* button, bool enabled) {
  if (!button) return;
  if (enabled) lv_obj_remove_state(button, LV_STATE_DISABLED);
  else lv_obj_add_state(button, LV_STATE_DISABLED);
}
static void setIconButtonSymbol(lv_obj_t* button, const char* symbol) {
  if (!button) return;
  lv_obj_t* label = lv_obj_get_child(button, 0);
  if (label) setLabelIfChanged(label, symbol);
}
static void updateAudioUi(bool force) {
  if (!audioPage) return;
  const uint32_t now = millis();
  const bool engineDirty = Tab5Audio.takeUiDirty();
  if (!force && !engineDirty && now - lastAudioUiUpdateMs < 250) return;
  lastAudioUiUpdateMs = now;

  static Tab5AudioSnapshot audioState;
  if (!Tab5Audio.snapshot(audioState)) return;
  audioRememberPlayback(audioState);
  Tab5VideoSnapshot videoState;
  const bool videoView = audioView == AudioView::Videos;
  const bool haveVideoState = videoView && Tab5Video.snapshot(videoState);

  String current = haveVideoState && videoState.name.length()
      ? videoState.name : audioState.nowPlaying;
  if((!videoView&&audioState.mode==Tab5AudioMode::Idle)||(videoView&&(!haveVideoState||videoState.state==Tab5VideoState::Idle)))
    current=Tab5MediaSession::state().title[unsigned(audioView)];
  if (!current.length())
    current = videoView ? T("Sélectionnez une vidéo") : T("Aucune lecture");
  setLabelIfChanged(audioNowPlaying, current.c_str());

  String state = haveVideoState ? videoState.status : audioState.status;
  if (videoView && (!haveVideoState || videoState.state == Tab5VideoState::Idle))
    state = "";
  if((!videoView&&audioState.mode==Tab5AudioMode::Idle)||(videoView&&(!haveVideoState||videoState.state==Tab5VideoState::Idle)))
    if(Tab5MediaSession::state().selected[unsigned(audioView)][0])state=T("Sélection mémorisée");
  if (Tab5Locale.hasTranslations()) {
    const String translatedState = translateRuntimeText(state);
    setLabelIfChanged(audioStatus, translatedState.c_str());
  } else {
    setLabelIfChanged(audioStatus, state.c_str());
  }

  uint16_t progress = 0;
  uint32_t elapsedSeconds = 0;
  uint32_t totalSeconds = 0;
  if (audioState.recording) {
    elapsedSeconds = audioState.recordingSeconds;
  } else {
    progress = audioState.playbackProgressPermille;
    elapsedSeconds = audioState.playbackElapsedSeconds;
    totalSeconds = audioState.playbackEstimatedSeconds;
  }
  const bool canSeek = audioState.mode == Tab5AudioMode::Local &&
      totalSeconds > 0 && audioPathIsSeekable(audioState.currentPath);
  setButtonEnabled(audioProgressSlider, canSeek);
  if (!audioProgressDragging)
    lv_slider_set_value(audioProgressSlider, progress, LV_ANIM_OFF);
  char timeText[40];
  if (audioState.mode == Tab5AudioMode::Radio) {
    snprintf(timeText, sizeof(timeText), T("DIRECT  •  %02lu:%02lu"),
             static_cast<unsigned long>(elapsedSeconds / 60),
             static_cast<unsigned long>(elapsedSeconds % 60));
  } else if (audioState.recording) {
    snprintf(timeText, sizeof(timeText), T("REC  •  %02lu:%02lu"),
             static_cast<unsigned long>(elapsedSeconds / 60),
             static_cast<unsigned long>(elapsedSeconds % 60));
  } else if (audioState.mode == Tab5AudioMode::Local && totalSeconds) {
    snprintf(timeText, sizeof(timeText), "%02lu:%02lu  /  %02lu:%02lu",
             static_cast<unsigned long>(elapsedSeconds / 60),
             static_cast<unsigned long>(elapsedSeconds % 60),
             static_cast<unsigned long>(totalSeconds / 60),
             static_cast<unsigned long>(totalSeconds % 60));
  } else {
    snprintf(timeText, sizeof(timeText), "%02lu:%02lu",
             static_cast<unsigned long>(elapsedSeconds / 60),
             static_cast<unsigned long>(elapsedSeconds % 60));
  }
  setLabelIfChanged(audioTimeLabel, timeText);

  setIconButtonSymbol(audioPlayBtn,
      audioState.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  setButtonEnabled(audioPlayBtn, !audioState.recording && !videoView);
  setTopActionVisible(audioStopBtn, audioState.mode != Tab5AudioMode::Idle);

  const bool recording = audioState.recording;
  lv_obj_t* recordLabel = audioRecordBtn ? lv_obj_get_child(audioRecordBtn, 0) : nullptr;
  if (recordLabel) {
    if (recording) {
      setLabelIfChanged(recordLabel, LV_SYMBOL_STOP);
      lv_obj_remove_flag(recordLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(recordLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (audioRecordGraphic) {
    if (recording) lv_obj_add_flag(audioRecordGraphic, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(audioRecordGraphic, LV_OBJ_FLAG_HIDDEN);
  }
  const bool showRecordControls = audioView == AudioView::Recordings || recording;
  setTopActionVisible(audioRecordControls, showRecordControls);
  const int volumeY = showRecordControls ? 490 : 390;
  if (audioVolumeIcon) lv_obj_set_pos(audioVolumeIcon, 2, volumeY);
  if (audioVolumeValue &&
      (lv_obj_get_style_align(audioVolumeValue, LV_PART_MAIN) != LV_ALIGN_TOP_RIGHT ||
       lv_obj_get_style_x(audioVolumeValue, LV_PART_MAIN) != -2 ||
       lv_obj_get_style_y(audioVolumeValue, LV_PART_MAIN) != volumeY))
    lv_obj_align(audioVolumeValue, LV_ALIGN_TOP_RIGHT, -2, volumeY);
  if (audioVolumeSlider) lv_obj_set_pos(audioVolumeSlider, 2, volumeY + 48);
  lv_bar_set_value(audioLevelBar, audioState.recordingLevel, LV_ANIM_OFF);
  if (audioVolumeSlider &&
      lv_slider_get_value(audioVolumeSlider) != audioState.volume) {
    lv_slider_set_value(audioVolumeSlider, audioState.volume, LV_ANIM_OFF);
  }
  char volumeText[16];
  snprintf(volumeText, sizeof(volumeText), "%u%%",
           static_cast<unsigned>((audioState.volume * 100U) / 255U));
  setLabelIfChanged(audioVolumeValue, volumeText);
  updateTopAudioIndicator(true);
}