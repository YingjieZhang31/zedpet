// claude-web frontend — vanilla JS, no build step.
const el = (id) => document.getElementById(id);
const messagesEl = el("messages");
const inputEl = el("input");
const sendBtn = el("send");
const formEl = el("form");
const connEl = el("conn");
const cwdEl = el("cwd");
const newSessionBtn = el("new-session");
const modeToggle = el("mode-toggle");
const footerInfo = el("footer-info");

let ws = null;
let reconnectDelay = 500;
let cardMode = false;

function setConn(state) {
  const styles = {
    connecting: ["bg-yellow-100 text-yellow-800", "connecting…"],
    open:       ["bg-green-100 text-green-800",  "connected"],
    closed:     ["bg-red-100 text-red-800",      "disconnected"],
  };
  const [cls, label] = styles[state];
  connEl.className = `text-xs px-2 py-0.5 rounded ${cls}`;
  connEl.textContent = label;
  sendBtn.disabled = state !== "open";
}

function connect() {
  setConn("connecting");
  const proto = location.protocol === "https:" ? "wss" : "ws";
  ws = new WebSocket(`${proto}://${location.host}/ws`);

  ws.onopen = () => {
    setConn("open");
    reconnectDelay = 500;
  };
  ws.onclose = () => {
    setConn("closed");
    setTimeout(connect, reconnectDelay);
    reconnectDelay = Math.min(reconnectDelay * 2, 10_000);
  };
  ws.onerror = () => { /* onclose will fire after */ };
  ws.onmessage = (e) => {
    let ev; try { ev = JSON.parse(e.data); } catch { return; }
    handleEvent(ev);
  };
}

function send(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
}

formEl.addEventListener("submit", (e) => {
  e.preventDefault();
  const text = inputEl.value.trim();
  if (!text) return;
  appendUserBubble(text);
  send({ type: "user_message", text });
  inputEl.value = "";
  sendBtn.disabled = true;
});

inputEl.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey) {
    e.preventDefault();
    formEl.requestSubmit();
  }
});

newSessionBtn.addEventListener("click", () => {
  if (!confirm("Start a new session and discard the current conversation?")) return;
  send({ type: "new_session" });
  messagesEl.innerHTML = "";
});

modeToggle.addEventListener("change", () => {
  cardMode = modeToggle.checked;
  document.body.dataset.toolMode = cardMode ? "card" : "compact";
});
document.body.dataset.toolMode = "compact";

// Placeholders to be implemented in later tasks.
function handleEvent(ev) {
  console.log("[event]", ev);
  if (ev.type === "ready") {
    cwdEl.textContent = `cwd: ${ev.cwd}` + (ev.session_id ? ` · session ${ev.session_id.slice(0, 8)}` : " · new session");
  }
}
function appendUserBubble(text) { /* Task 9 */ }

connect();
