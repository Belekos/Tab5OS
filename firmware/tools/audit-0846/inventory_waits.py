#!/usr/bin/env python3
"""Reproducible lexical wait/task/allocation inventory; not a latency benchmark.

Only comments/string literals are masked; preprocessor alternatives are retained
and labelled. No occurrence is asserted reachable merely because it is listed.
"""
import argparse
import bisect
import collections
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import zipfile

SUFFIXES = {'.c', '.cpp', '.h', '.hpp', '.inc', '.ino'}
WAIT_NAMES = {
    'delay', 'delayMicroseconds', 'vTaskDelay', 'vTaskDelayUntil',
    'xTaskDelayUntil', 'ulTaskNotifyTake', 'xTaskNotifyWait',
    'xSemaphoreTake', 'xSemaphoreTakeRecursive', 'xQueueReceive',
    'xQueueSend', 'xQueueSendToBack', 'xQueueSendToFront', 'xQueuePeek',
    'xStreamBufferReceive', 'xStreamBufferSend', 'xMessageBufferReceive', 'xMessageBufferSend',
    'ulTaskNotifyTakeIndexed', 'xTaskNotifyWaitIndexed', 'xEventGroupSync',
    'xRingbufferReceive', 'xRingbufferReceiveUpTo', 'xRingbufferSend',
    'xEventGroupWaitBits', 'vTaskSuspend', 'taskYIELD', 'usleep',
    'nanosleep', 'sleep', 'pauseAdmission', 'yield', 'esp_rom_delay_us', 'ets_delay_us',
    'usb_host_lib_handle_events', 'usb_host_client_handle_events',
    'uart_wait_tx_done', 'select', 'poll', 'pthread_cond_wait',
    'pthread_cond_timedwait', 'pthread_mutex_lock',
}
TASK_NAMES = {
    'tab5CreateTask', 'tab5CreateAdmittedTask', 'xTaskCreate',
    'xTaskCreatePinnedToCore', 'xTaskCreatePinnedToCoreWithCaps',
    'xTaskCreateStatic', 'xTaskCreateStaticPinnedToCore',
}
ALLOC_NAMES = {
    'heap_caps_malloc', 'heap_caps_calloc', 'heap_caps_realloc',
    'heap_caps_aligned_alloc', 'heap_caps_aligned_calloc',
    'heap_caps_get_largest_free_block', 'tab5ReserveMemory',
}
INDIRECT_NAMES = {'Tab5ImageLock', 'Tab5NetworkLease', 'Tab5NetworkTransition'}


def scope(name):
    p = Path(name)
    if p.suffix not in SUFFIXES:
        return False
    if len(p.parts) == 1:
        return True
    return p.parts[0] in {'src', 'py'} or name in {
        'tools/native-apps/image-editor/image_core.c',
        'tools/native-apps/image-editor/image_core.h',
    }


def mask(text):
    # C/C++ raw strings first, then ordinary strings/chars and comments.
    pattern = re.compile(
        r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)"'
        r'|"(?:\\[\s\S]|[^"\\])*"|\'(?:\\[\s\S]|[^\'\\])*\''
        r'|//[^\n]*|/\*[\s\S]*?\*/')
    return pattern.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m.group()), text)


def arguments(clean, text, at):
    depth = 0
    for end in range(at, len(clean)):
        if clean[end] == '(':
            depth += 1
        elif clean[end] == ')':
            depth -= 1
            if not depth:
                return text[at + 1:end], end + 1
    return '', at


def category(api, args, filename):
    compact = re.sub(r'\s+', '', args)
    if api in TASK_NAMES:
        return 'task_creation'
    if api in ALLOC_NAMES:
        return 'memory_capabilities_or_admission'
    if api in INDIRECT_NAMES:
        return 'blocking_wrapper_review_call_context'
    if api in {'vTaskSuspend'}:
        return 'intentional_task_suspension'
    if api in {'delayMicroseconds', 'usleep', 'nanosleep', 'sleep', 'esp_rom_delay_us', 'ets_delay_us'}:
        return 'delay_sdk_semantics_required'
    if api in {'delay', 'vTaskDelay', 'vTaskDelayUntil', 'xTaskDelayUntil', 'pauseAdmission', 'taskYIELD', 'yield'}:
        if compact == '0':
            return 'zero_tick_yield_no_idle_guarantee'
        return 'scheduler_delay_review_context'
    if compact.endswith(',0'):
        return 'nonblocking_poll_or_send'
    if 'portMAX_DELAY' in args:
        return 'unbounded_event_or_mutex_wait_review_owner'
    return 'bounded_or_parameterized_wait_review_owner'


def describe_context(name):
    if name.startswith('src/') or name.startswith('py/'):
        return 'embedded_dependency_or_port; caller-specific'
    if name.endswith('.ino') or name.endswith('.inc'):
        return 'UI/setup translation unit; inspect enclosing function'
    if name in {'Tab5ImageEditor.cpp', 'Tab5Writing.cpp', 'Tab5PythonReader.cpp'}:
        return 'UI-facing application; a scheduler yield does not return to LVGL'
    if name in {'Tab5ResourceManager.cpp', 'Tab5Task.h'}:
        return 'shared UI/worker infrastructure'
    return 'module API; may have UI and worker callers'


def scan(files, origin):
    entries, loops, sources = [], [], []
    names = WAIT_NAMES | TASK_NAMES | ALLOC_NAMES | INDIRECT_NAMES
    calls = re.compile(r'\b(' + '|'.join(sorted(names, key=len, reverse=True)) + r')\s*\(')
    for name, raw in sorted(files.items()):
        if not scope(name):
            continue
        text = raw.decode('utf-8', errors='replace').replace('\r', '')
        clean = mask(text)
        starts = [0] + [m.end() for m in re.finditer('\n', text)]
        sources.append({'path': name, 'sha256': hashlib.sha256(raw).hexdigest(), 'bytes': len(raw)})
        for m in calls.finditer(clean):
            at = clean.find('(', m.start(), m.end())
            args, end = arguments(clean, text, at)
            after = clean[end:].lstrip()
            # Ordinary declarations/definitions are not runtime calls.
            # Wrapper templates are intentionally included with this marker.
            definition = after.startswith('{') and m.group(1) in {'delay', 'pauseAdmission'}
            if definition:
                continue
            entries.append({
                'file': name, 'line': bisect.bisect_right(starts, m.start()),
                'api': m.group(1), 'arguments': ' '.join(args.split()),
                'category': category(m.group(1), args, name),
                'context': describe_context(name),
                'reachability': 'lexical; preprocessor branches and template declarations retained',
            })
        wrappers = re.compile(r'\b(' + '|'.join(sorted(INDIRECT_NAMES)) + r')\s+\w+\s*\(')
        for m in wrappers.finditer(clean):
            at = clean.find('(', m.start(), m.end())
            args, end = arguments(clean, text, at)
            entries.append({'file': name, 'line': bisect.bisect_right(starts, m.start()),
                'api': m.group(1), 'arguments': ' '.join(args.split()),
                'category': 'blocking_wrapper_review_call_context', 'context': describe_context(name),
                'reachability': 'lexical constructor with named instance; inspect callers'})
        for m in re.finditer(r'\b(while|for)\s*\(', clean):
            at = clean.find('(', m.start(), m.end())
            args, end = arguments(clean, text, at)
            relevant = re.search(r'millis|micros|Tick|busy\s*\(|done|exited|ready|running|acquire\s*\(', args, re.I)
            infinite = re.sub(r'\s+', '', args) in {'true', '1', ';;'}
            if relevant or infinite:
                loops.append({'file': name, 'line': bisect.bisect_right(starts, m.start()),
                    'kind': m.group(1), 'condition': ' '.join(args.split()),
                    'review': 'candidate only: event task, parser loop, bounded work or timed wait must be distinguished'})
    return {'schema': 1, 'origin': origin, 'limitations': [
        'Lexical inventory, not proof that each occurrence is compiled or reachable.',
        'All top-level application sources/headers, src/ ports/dependencies, py/ headers and compiled image_core are covered.',
        'Historical test trees, independent ELF sample apps, external SDK and dynamic Python programs are excluded.',
        'Indirect I/O, HTTP, filesystem, driver waits and virtual calls require the accompanying manual audit.',
        'No board runtime duration or CPU percentage was measured.'
    ], 'source_count': len(sources), 'counts_by_api': dict(sorted(collections.Counter(x['api'] for x in entries).items())),
        'counts_by_category': dict(sorted(collections.Counter(x['category'] for x in entries).items())),
        'sources': sources, 'calls': entries, 'candidate_loops': loops}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sketch', required=True, type=Path)
    parser.add_argument('--baseline-zip', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sources = {p.relative_to(args.sketch).as_posix(): p.read_bytes()
        for p in args.sketch.rglob('*') if p.is_file() and scope(p.relative_to(args.sketch).as_posix())}
    current = scan(sources, str(args.sketch))
    (args.output / 'waits-memory-inventory-current.json').write_text(json.dumps(current, indent=2, ensure_ascii=False) + '\n')
    with (args.output / 'waits-memory-inventory-current.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['file', 'line', 'api', 'arguments', 'category', 'context', 'reachability'])
        writer.writeheader(); writer.writerows(current['calls'])
    if args.baseline_zip:
        with zipfile.ZipFile(args.baseline_zip) as z:
            prefix = next(n for n in z.namelist() if n.endswith('Tab5OS_Arduino_v0.8.45_Ecriture.ino')).rsplit('/', 1)[0] + '/'
            baseline = {n[len(prefix):]: z.read(n) for n in z.namelist()
                if n.startswith(prefix) and scope(n[len(prefix):])}
        previous = scan(baseline, {'zip': args.baseline_zip.name, 'sha256': hashlib.sha256(args.baseline_zip.read_bytes()).hexdigest()})
        (args.output / 'waits-memory-inventory-0845.json').write_text(json.dumps(previous, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps({'sources': current['source_count'], 'calls': len(current['calls']),
        'candidate_loops': len(current['candidate_loops']), 'by_category': current['counts_by_category']}, ensure_ascii=False))


if __name__ == '__main__':
    main()
