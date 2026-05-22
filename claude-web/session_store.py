"""Persistence of the latest Claude Code session_id."""
from __future__ import annotations

import json
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path


class SessionStore:
    def __init__(self, path: Path):
        self._path = Path(path)

    def load(self) -> str | None:
        if not self._path.exists():
            return None
        try:
            data = json.loads(self._path.read_text())
        except (json.JSONDecodeError, OSError):
            return None
        sid = data.get("session_id")
        return sid if isinstance(sid, str) and sid else None

    def save(self, session_id: str) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "session_id": session_id,
            "updated_at": datetime.now(timezone.utc).isoformat(),
        }
        fd, tmp_name = tempfile.mkstemp(
            prefix=".session-", suffix=".json.tmp", dir=str(self._path.parent)
        )
        tmp = Path(tmp_name)
        try:
            with os.fdopen(fd, "w") as f:
                json.dump(payload, f)
            tmp.replace(self._path)
        except Exception:
            if tmp.exists():
                tmp.unlink(missing_ok=True)
            raise

    def clear(self) -> None:
        try:
            self._path.unlink()
        except FileNotFoundError:
            pass
