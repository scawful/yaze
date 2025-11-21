#!/usr/bin/env python3
"""Stream appended entries from the coordination board in near real time."""

import argparse
import os
import random
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Optional


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Stream appended entries from docs/internal/agents/coordination-board.md "
            "so agents can follow each other's updates."
        )
    )
    parser.add_argument(
        "board_path",
        nargs="?",
        default="docs/internal/agents/coordination-board.md",
        help="Path to the coordination board (default: %(default)s)",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=2.0,
        help="Polling interval in seconds (default: %(default)s). "
        "Use higher values (2-5s) to minimize CPU usage.",
    )
    parser.add_argument(
        "--from-start",
        action="store_true",
        help="Print the entire board before streaming new updates.",
    )
    parser.add_argument(
        "--highlight-keyword",
        action="append",
        dest="highlight_keywords",
        default=[],
        help=(
            "Keyword to highlight (case-insensitive). "
            "May be specified multiple times. Defaults include "
            "'keep chatting', 'council vote', 'request → all', and 'need more agents'."
        ),
    )
    parser.add_argument(
        "--topic-file",
        default="docs/internal/agents/yaze-keep-chatting-topics.md",
        help=(
            "Optional file of keep-chatting topics/busy tasks "
            "(default: %(default)s). Use 'none' to disable."
        ),
    )
    parser.add_argument(
        "--no-prompts",
        action="store_true",
        help=(
            "Disable automatic busy-task/topic prompts when the stream detects "
            "'keep chatting' entries."
        ),
    )
    parser.add_argument(
        "--log-file",
        default=None,
        help=(
            "Optional path to append each new chunk with a timestamp "
            "for offline auditing."
        ),
    )
    return parser.parse_args()


def normalize_keywords(raw_keywords):
    base = [
        "keep chatting",
        "council vote",
        "request → all",
        "request -> all",
        "need more agents",
        "poll →",
        "poll ->",
    ]
    combined = base + (raw_keywords or [])
    return [kw.lower() for kw in combined if kw.strip()]


BUSY_TASKS = [
    "archive stale board entries",
    "summarize the latest CI failure",
    "update the release checklist",
    "polish a helper script or preset",
    "draft a dungeon/graphics/sprite TODO",
    "log a build timing for the coordinator",
    "write a tiny doc clarification",
    "check canonical run status and post it",
    "start a poll or council vote recap",
]


def load_topics(topic_file: str) -> list[str]:
    if not topic_file or topic_file.lower() == "none":
        return []
    path = Path(topic_file)
    if not path.exists():
        print(
            f"[stream-board] warning: topic file not found: {path}",
            file=sys.stderr,
        )
        return []
    topics: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith(("#", ">")):
            continue
        for prefix in ("- ", "* ", "• ", "1. ", "2. ", "3. "):
            if stripped.startswith(prefix):
                topics.append(stripped[len(prefix) :].strip())
                break
    return topics


def build_prompt(topics: list[str]) -> str:
    busy = random.choice(BUSY_TASKS)
    topic = random.choice(topics) if topics else None
    if topic:
        return (
            f"Grab a busy task ({busy}) + riff on “{topic}” before your next board post."
        )
    return f"Grab a busy task ({busy}) before your next board post."


def maybe_emit_prompt(chunk: str, topics: list[str], disable_prompts: bool) -> None:
    if disable_prompts:
        return
    if "keep chatting" not in chunk.lower():
        return
    prompt = build_prompt(topics)
    print(f"\n[stream-board] keep chatting signal → {prompt}\n")


def apply_highlights(chunk: str, keywords):
    if not keywords:
        return chunk
    lines = chunk.splitlines(keepends=True)
    highlighted = []
    for line in lines:
        lower = line.lower()
        matches = [kw for kw in keywords if kw in lower]
        if matches:
            tag = ", ".join(matches)
            highlighted.append(f"[HIGHLIGHT {tag}] {line}")
        else:
            highlighted.append(line)
    return "".join(highlighted)


def stream_board(
    board_path: Path,
    interval: float,
    from_start: bool,
    keywords,
    topics,
    disable_prompts: bool,
    log_file: Optional[Path],
) -> int:
    if not board_path.exists():
        print(f"[stream-board] error: file not found: {board_path}", file=sys.stderr)
        return 1

    print(f"[stream-board] Watching {board_path} (Ctrl+C to stop)")
    if log_file:
        log_file.parent.mkdir(parents=True, exist_ok=True)
        print(f"[stream-board] Logging new chunks to {log_file}")
    last_inode = None
    offset = 0

    while True:
        try:
            stat = board_path.stat()
        except FileNotFoundError:
            print(
                "[stream-board] board missing; waiting for it to reappear...",
                file=sys.stderr,
            )
            time.sleep(interval)
            continue

        if last_inode != stat.st_ino or stat.st_size < offset:
            # File rotated or truncated; restart from beginning or end depending on flag.
            last_inode = stat.st_ino
            offset = 0 if from_start else stat.st_size
            if not from_start:
                print(
                    "[stream-board] board truncated or rotated; "
                    "skipping to current end. Use --from-start to replay.",
                    file=sys.stderr,
                )

        with board_path.open("r", encoding="utf-8") as board_file:
            board_file.seek(offset)
            chunk = board_file.read()
            offset = board_file.tell()

        if chunk:
            highlighted = apply_highlights(chunk, keywords)
            sys.stdout.write(highlighted)
            sys.stdout.flush()
            maybe_emit_prompt(chunk, topics, disable_prompts)
            if log_file:
                timestamp = datetime.now().isoformat(timespec="seconds")
                with log_file.open("a", encoding="utf-8") as log_handle:
                    log_handle.write(f"\n--- {timestamp} ---\n")
                    log_handle.write(chunk)

        from_start = False
        time.sleep(interval)


def main() -> int:
    args = parse_args()
    board_path = Path(args.board_path)
    try:
        keywords = normalize_keywords(args.highlight_keywords)
        topics = load_topics(args.topic_file)
        log_path = Path(args.log_file) if args.log_file else None
        return stream_board(
            board_path,
            args.interval,
            args.from_start,
            keywords,
            topics,
            args.no_prompts,
            log_path,
        )
    except KeyboardInterrupt:
        print("\n[stream-board] stopped.", file=sys.stderr)
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
