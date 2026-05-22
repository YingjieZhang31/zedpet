import json
from pathlib import Path

from session_store import SessionStore


def test_load_missing_file_returns_none(tmp_path: Path):
    store = SessionStore(tmp_path / "session.json")
    assert store.load() is None


def test_save_then_load_roundtrip(tmp_path: Path):
    store = SessionStore(tmp_path / "session.json")
    store.save("abc123")
    assert store.load() == "abc123"


def test_save_overwrites_previous(tmp_path: Path):
    store = SessionStore(tmp_path / "session.json")
    store.save("first")
    store.save("second")
    assert store.load() == "second"


def test_save_writes_updated_at(tmp_path: Path):
    path = tmp_path / "session.json"
    store = SessionStore(path)
    store.save("xyz")
    data = json.loads(path.read_text())
    assert data["session_id"] == "xyz"
    assert "updated_at" in data


def test_clear_removes_file(tmp_path: Path):
    path = tmp_path / "session.json"
    store = SessionStore(path)
    store.save("xyz")
    store.clear()
    assert not path.exists()
    assert store.load() is None


def test_clear_when_missing_is_noop(tmp_path: Path):
    store = SessionStore(tmp_path / "session.json")
    store.clear()  # must not raise


def test_atomic_write_preserves_old_on_crash(tmp_path: Path, monkeypatch):
    """If rename fails mid-save, the old file must remain intact."""
    path = tmp_path / "session.json"
    store = SessionStore(path)
    store.save("original")

    real_replace = Path.replace
    def boom(self, target):
        raise OSError("simulated crash")
    monkeypatch.setattr(Path, "replace", boom)

    try:
        store.save("new")
    except OSError:
        pass

    monkeypatch.setattr(Path, "replace", real_replace)
    assert store.load() == "original"


def test_load_corrupt_file_returns_none(tmp_path: Path):
    path = tmp_path / "session.json"
    path.write_text("not json")
    store = SessionStore(path)
    assert store.load() is None
