"""FastAPI app: serves the web UI and bridges WebSocket to ClaudeRunner."""
from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

import uvicorn
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, StreamingResponse
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

    ask_runner = ClaudeRunner(
        cwd=cwd,
        session_store=SessionStore(cwd / ".claude-web" / "ask-session.json"),
    )
    app.state.ask_runner = ask_runner

    app.mount("/static", StaticFiles(directory=static_dir), name="static")

    @app.get("/")
    def index():
        return FileResponse(static_dir / "index.html")

    @app.websocket("/ws")
    async def ws_endpoint(ws: WebSocket):
        await ws.accept()
        runner: ClaudeRunner = app.state.runner
        await ws.send_json({
            "type": "ready",
            "session_id": runner.current_session_id(),
            "cwd": str(runner.cwd),
        })
        try:
            while True:
                raw = await ws.receive_text()
                try:
                    msg = json.loads(raw)
                except json.JSONDecodeError:
                    await ws.send_json({"type": "error", "message": "invalid JSON"})
                    continue

                mtype = msg.get("type")
                if mtype == "user_message":
                    text = msg.get("text", "")
                    if not isinstance(text, str) or not text.strip():
                        await ws.send_json({"type": "error", "message": "empty prompt"})
                        continue
                    async for ev in runner.query(text):
                        await ws.send_json(ev)
                elif mtype == "new_session":
                    runner.reset_session()
                    await ws.send_json({
                        "type": "ready",
                        "session_id": None,
                        "cwd": str(runner.cwd),
                    })
                else:
                    await ws.send_json({
                        "type": "error",
                        "message": f"unknown message type: {mtype!r}",
                    })
        except WebSocketDisconnect:
            return

    @app.post("/ask")
    async def ask(payload: dict):
        text = (payload or {}).get("text", "")
        if not isinstance(text, str) or not text.strip():
            raise HTTPException(status_code=400, detail="missing 'text'")

        async def stream():
            async for ev in app.state.ask_runner.query(text):
                if ev["type"] == "assistant_text":
                    yield ev["text"]
                elif ev["type"] == "error":
                    yield f"\n[error: {ev['message']}]\n"

        return StreamingResponse(stream(), media_type="text/plain; charset=utf-8")

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
