"""Prepare M5Unified 0.2.23 with clocked startup and acknowledged PCM stop."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

EXPECTED = {
    'src/utility/Speaker_Class.hpp': '886ad34c1d2def4d0cb550368495de28c6cc580ed14f0f0454ef201bfd2c9d84',
    'src/utility/Speaker_Class.inl': '1229920ffe04ccef8f59c37d2dc848a5d34dfd5d5424639bc46c13102eb6fc17',
    'src/M5Unified.inl': '6a42347d7fad2b77c96874a7338cd6b8cf04de5863ed4013a23c3b605efb0bfb',
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise RuntimeError('Unexpected M5Unified source near: ' + before[:100])
    return text.replace(before, after)


def patched_files(source):
    result = {}
    for name, expected in EXPECTED.items():
        original = (source/name).read_bytes()
        if digest(original) != expected:
            raise RuntimeError('Refusing unchecked M5Unified patch: ' + name)
        result[name] = original.decode()

    name = 'src/utility/Speaker_Class.hpp'
    result[name] = replace_once(result[name], '  struct speaker_config_t\n',
        '#define M5UNIFIED_HAS_CLOCKED_ENABLE 1\n\n  struct speaker_config_t\n')
    result[name] = replace_once(result[name],
        '    /// I2S port\n    i2s_port_t i2s_port',
        '    /// External codec: establish clocks and a silent DMA cycle before enable.\n'
        '    /// Opt-in; internal DAC/buzzer and other boards keep their startup path.\n'
        '    bool enable_after_i2s = false;\n\n'
        '    /// I2S port\n    i2s_port_t i2s_port')

    result[name] = replace_once(result[name],
        '    void stop(uint8_t channel);',
        '    void stop(uint8_t channel);\n\n'
        '    /// Stop the exclusively owned channel and wait until its PCM references retire.\n'
        '    /// Clocks/DMA remain active. Never call from the playback/release callback.\n'
        '    bool stopAndWait(uint8_t channel);')

    name = 'src/utility/Speaker_Class.inl'
    result[name] = replace_once(result[name],
        '      chinfo->current.clear();\n',
        '      chinfo->current.clear();\n'
        '      // The worker has joined: no interpolation state may cross end()/begin().\n'
        '      // Keep channel volume and configuration; continuous playback is untouched.\n'
        '      chinfo->index = 0;\n'
        '      chinfo->diff = 0;\n'
        '      memset(chinfo->liner_buf, 0, sizeof(chinfo->liner_buf));\n')
    result[name] = replace_once(result[name],
        '    if (_task_semaphore == nullptr) { _task_semaphore = xSemaphoreCreateBinary(); }',
        '    if (_task_semaphore == nullptr) { _task_semaphore = xSemaphoreCreateBinary(); }\n'
        '    if (_task_semaphore == nullptr) { _begin_lock.store(false); return false; }')
    result[name] = replace_once(result[name],
        '    spk_i2s_state flg_i2s_started = spk_i2s_stop;',
        '    spk_i2s_state flg_i2s_started = self->_cfg.enable_after_i2s\n'
        '        ? spk_i2s_run : spk_i2s_stop;')
    result[name] = replace_once(result[name],
        '''    bool res = true;
    if (_cb_set_enabled) { res = _cb_set_enabled(_cb_set_enabled_args, true); }

    res = (ESP_OK == _setup_i2s()) && res;
''', '''    bool deferred_enable = false;
#if !defined (SDL_h_)
    deferred_enable = _cfg.enable_after_i2s;
#endif
    bool res = true;
    bool startup_cleaned = false;
    if (_cb_set_enabled) { res = _cb_set_enabled(_cb_set_enabled_args, !deferred_enable); }

    if (!deferred_enable || res) { res = (ESP_OK == _setup_i2s()) && res; }
#if !defined (SDL_h_)
    if (res && deferred_enable)
    {
      // An external codec must never be unmuted while its clocks are absent.
      // A whole DMA traversal of zero samples establishes clocks/data before
      // its enable callback. This is transfer back-pressure, not a timed sleep.
      res = !_cfg.use_dac && !_cfg.buzzer && _cfg.dma_buf_len > 0
          && _cfg.dma_buf_len <= 1024 && _cfg.dma_buf_count >= 2
          && _cfg.dma_buf_count <= 128;
      size_t remaining = 0;
#if __has_include(<driver/i2s_std.h>)
      i2s_chan_info_t info = {};
      if (res) { res = ESP_OK == i2s_channel_get_info(_spk_i2s_handle[_cfg.i2s_port], &info); }
      if (res)
      {
        // IDF can align the requested frame count. Use its actual DMA size.
        const size_t block = info.total_dma_buf_size / _cfg.dma_buf_count;
        res = block > 0 && info.total_dma_buf_size % _cfg.dma_buf_count == 0
            && info.total_dma_buf_size <= SIZE_MAX - block;
        if (res) { remaining = info.total_dma_buf_size + block; }
      }
#else
      res = false; // Clocked enable requires the native channel information API.
#endif
      if (res) { res = (ESP_OK == _spk_i2s_start(_cfg.i2s_port)); }
      if (res)
      {
        uint32_t silence[64] = {};
        while (res && remaining)
        {
          size_t written = 0;
          const size_t amount = std::min(remaining, sizeof(silence));
          // i2s_channel_write uses milliseconds; failure is propagated, never
          // followed by an amplifier enable or a partially initialized task.
          res = ESP_OK == _spk_i2s_write(_cfg.i2s_port, silence, amount, &written, 1000);
          res = res && written > 0 && written <= amount;
          if (res) { remaining -= written; }
        }
      }
      if (res && _cb_set_enabled) { res = _cb_set_enabled(_cb_set_enabled_args, true); }
    }
#endif
''')
    result[name] = replace_once(result[name],
        '      if (!res) { _end_locked(); }',
        '      if (!res) { _end_locked(); startup_cleaned = true; }')
    result[name] = replace_once(result[name],
        '''    _begin_lock.store(false);

    return res;
  }

  void Speaker_Class::end(void)''',
        '''#if !defined (SDL_h_)
    // No worker exists on setup/prime/codec failure. A clocked port must be
    // stopped explicitly before uninstall; _end_locked handles task failure.
    if (!res && deferred_enable && !startup_cleaned)
    {
      if (_cb_set_enabled) { _cb_set_enabled(_cb_set_enabled_args, false); }
      _spk_i2s_stop(_cfg.i2s_port);
      _spk_i2s_driver_uninstall(_cfg.i2s_port);
    }
#endif
    _begin_lock.store(false);

    return res;
  }

  void Speaker_Class::end(void)''')
    result[name] = replace_once(result[name],
        '''#if !defined (SDL_h_)
    _spk_i2s_driver_uninstall(_cfg.i2s_port);
#endif
  }

  void Speaker_Class::stop(void)''',
        '''#if !defined (SDL_h_)
    // With deferred enable, I2S may be running even if task creation failed.
    if (_cfg.enable_after_i2s) { _spk_i2s_stop(_cfg.i2s_port); }
    _spk_i2s_driver_uninstall(_cfg.i2s_port);
#endif
  }

  void Speaker_Class::stop(void)''')

    result[name] = replace_once(result[name],
        '              current_wav->clear();\n',
        '              current_wav->clear();\n'
        '              ch_index = 0;\n'
        '              ch_diff = 0;\n'
        '              memset(ch_info->liner_buf, 0, sizeof(ch_info->liner_buf));\n')
    result[name] = replace_once(result[name],
        '            self->_play_channel_bits.fetch_and(~(1 << ch));',
        '            self->_play_channel_bits.fetch_and(~(1 << ch));\n'
        '#if !defined (SDL_h_)\n'
        '            // Publish retirement before waking a stopAndWait waiter.\n'
        '            xSemaphoreGive(self->_task_semaphore);\n'
        '#endif')
    result[name] = replace_once(result[name],
        '  void Speaker_Class::wav_info_t::clear(void)',
        '''  bool Speaker_Class::stopAndWait(uint8_t ch)
  {
    if ((size_t)ch >= sound_channel_max || _in_task()) { return false; }
    // Serialize producers and end(): after the marker, nobody may republish
    // a PCM pointer on this channel before its retirement is acknowledged.
    _lock_req();
    const uint16_t mask = 1U << ch;
    if (_begun.load(std::memory_order_acquire)
        && _task_handle.load(std::memory_order_acquire))
    {
      wav_info_t marker;
      marker.stop_current = 1;
      for (;;)
      {
        if (_set_next_wav_locked(ch, marker) >= 0) { break; }
#if defined (SDL_h_)
        SDL_Delay(1);
#else
        xSemaphoreTake(_task_semaphore, portMAX_DELAY);
#endif
      }
      // Natural EOF briefly clears the activity bit before adopting an
      // already published marker. Both slots must also have retired, or a
      // resumed producer could replace that marker before it was consumed.
      auto pending = [&]() {
        return (_play_channel_bits.load(std::memory_order_acquire) & mask)
            || (_ch_info[ch].wavinfo[0].state.load(std::memory_order_acquire)
                & wav_phase_mask) != wav_phase_empty
            || (_ch_info[ch].wavinfo[1].state.load(std::memory_order_acquire)
                & wav_phase_mask) != wav_phase_empty;
      };
      while (pending())
      {
#if defined (SDL_h_)
        SDL_Delay(1);
#else
        xSemaphoreTake(_task_semaphore, portMAX_DELAY);
#endif
      }
    }
    _req_lock.store(false, std::memory_order_release);
    return true;
  }

  void Speaker_Class::wav_info_t::clear(void)''')

    name = 'src/M5Unified.inl'
    start = result[name].index('  bool M5Unified::_speaker_enabled_cb_tab5(')
    end = result[name].index('\n  bool ', start + 10)
    before = result[name][start:end]
    after = replace_once(before, '      2,    4, 0x3C, //DAC POWER: power up and LOUT1/ROUT1/LOUT2/ROUT2 enable',
        '      2,   25, 0x24, // DAC soft mute before powering its outputs\n'
        '      2,    4, 0x3C, //DAC POWER: power up and LOUT1/ROUT1/LOUT2/ROUT2 enable')
    after = replace_once(after, '      2,   25, 0x20, //DAC unmute\n', '')
    after = replace_once(after, '''      in_i2c_bulk_write(es8388_i2c_addr, enabled_bulk_data);
      // AMP on
      M5.In_I2C.bitOn(pi4io1_i2c_addr, 0x05, 0b00000010, 400000);''',
        '''      // Keep the amplifier disabled throughout reset and DAC configuration.
      bool ok = M5.In_I2C.bitOff(pi4io1_i2c_addr, 0x05, 0b00000010, 400000);
      ok = in_i2c_bulk_write(es8388_i2c_addr, enabled_bulk_data) && ok;
      if (ok) { ok = M5.In_I2C.writeRegister8(es8388_i2c_addr, 25, 0x20, 400000); }
      if (ok) { ok = M5.In_I2C.bitOn(pi4io1_i2c_addr, 0x05, 0b00000010, 400000); }
      if (!ok)
      {
        M5.In_I2C.bitOff(pi4io1_i2c_addr, 0x05, 0b00000010, 400000);
        M5.In_I2C.writeRegister8(es8388_i2c_addr, 25, 0x24, 400000);
      }
      return ok;''')
    after = replace_once(after,
        '      M5.In_I2C.bitOff(pi4io1_i2c_addr, 0x05, 0b00000010, 400000); // AMP off',
        '      bool ok = M5.In_I2C.bitOff(pi4io1_i2c_addr, 0x05, 0b00000010, 400000); // AMP off')
    after = replace_once(after,
        '      M5.In_I2C.writeRegister8(es8388_i2c_addr, 25, 0x24, 400000); // DACCONTROL3',
        '      ok = M5.In_I2C.writeRegister8(es8388_i2c_addr, 25, 0x24, 400000) && ok; // DACCONTROL3')
    after = replace_once(after,
        '      M5.In_I2C.writeRegister8(es8388_i2c_addr,  4, 0xC0, 400000); // DACPOWER',
        '      ok = M5.In_I2C.writeRegister8(es8388_i2c_addr,  4, 0xC0, 400000) && ok; // DACPOWER')
    after = replace_once(after,
        '      M5.In_I2C.writeRegister8(es8388_i2c_addr,  2, 0xFF, 400000); // CHIPPOWER: 全停止 (ADF deinit と同一の終端状態)',
        '      ok = M5.In_I2C.writeRegister8(es8388_i2c_addr,  2, 0xFF, 400000) && ok; // CHIPPOWER\n'
        '      return ok;')
    result[name] = result[name][:start] + after + result[name][end:]
    return {name: text.encode() for name, text in result.items()}


def manifest(root):
    return {p.relative_to(root).as_posix(): digest(p.read_bytes())
            for p in sorted(root.rglob('*')) if p.is_file()}


def prepare_m5unified_audio(source, work):
    source, work = Path(source).resolve(), Path(work).resolve()
    patched = patched_files(source)
    expected = manifest(source)
    for name, data in patched.items():
        expected[name] = digest(data)
    destination = work/'dependencies/M5Unified'
    if destination.exists():
        if manifest(destination) != expected:
            raise RuntimeError('Existing isolated M5Unified differs; use a fresh work directory')
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source, destination)
        for name, data in patched.items():
            (destination/name).write_bytes(data)
    report = {'library': 'M5Unified', 'version': '0.2.23',
              'source': str(source), 'isolatedCopy': str(destination),
              'originalInstallationModified': False, 'filesChecked': len(expected),
              'files': [{'file': n, 'originalSha256': EXPECTED[n],
                         'patchedSha256': digest(v)} for n, v in patched.items()]}
    (work/'m5unified-audio-patch.json').write_text(json.dumps(report, indent=2)+'\n')
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--work', required=True, type=Path)
    args = parser.parse_args()
    print(prepare_m5unified_audio(args.source, args.work))
