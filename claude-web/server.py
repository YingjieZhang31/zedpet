"""FastAPI app: serves the web UI and bridges WebSocket to ClaudeRunner."""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

import uvicorn
from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from claude_runner import ClaudeRunner
from session_store import SessionStore


def build_app(cwd: Path) -> FastAPI:
    app = FastAPI(title="claude-web")
    static_dir = Path(__file__).parent / "static"

    runner = ClaudeRunner(
        cwd=cwd,
        session_store=SessionStore(cwd / ".claude-web" / "session.json"),
    )
    app.state.runner = runner

    app.mount("/static", StaticFiles(directory=static_dir), name="static")

    @app.get("/")
    def index():
        return FileResponse(static_dir / "index.html")

    return app


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="claude-web")
    p.add_argument("--cwd", required=True, type=Path,
                   help="Working directory Claude Code operates in")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", default=8000, type=int)
    return p.parse_args(argv)


def check_claude_binary() -> None:
    if shutil.which("claude") is None:
        sys.stderr.write(
            "Error: `claude` CLI not found in PATH.\n"
            "Install Claude Code: https://docs.claude.com/claude-code\n"
        )
        sys.exit(1)


def main(argv: list[str] | None = None) -> None:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    check_claude_binary()

    cwd = args.cwd.expanduser().resolve()
    if not cwd.is_dir():
        sys.stderr.write(f"Error: --cwd {cwd} is not a directory\n")
        sys.exit(1)

    print(f"claude-web: cwd={cwd}  http://{args.host}:{args.port}")
    uvicorn.run(build_app(cwd), host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
