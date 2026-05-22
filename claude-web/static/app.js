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

function appendUserBubble(text) {
  const div = document.createElement("div");
  div.className = "flex justify-end";
  div.innerHTML = `<div class="max-w-[80%] bg-blue-600 text-white rounded-lg px-3 py-2 text-sm whitespace-pre-wrap"></div>`;
  div.firstChild.textContent = text;
  messagesEl.appendChild(div);
  scrollToBottom();
}

function scrollToBottom() {
  messagesEl.scrollTop = messagesEl.scrollHeight;
}

const assistantBubbles = new Map();  // message_id -> <div> with the streaming text

function appendAssistantChunk(messageId, text) {
  let bubble = assistantBubbles.get(messageId);
  if (!bubble) {
    const wrap = document.createElement("div");
    wrap.className = "flex justify-start";
    wrap.innerHTML = `<div class="max-w-[80%] bg-white border rounded-lg px-3 py-2 text-sm whitespace-pre-wrap shadow-sm"></div>`;
    bubble = wrap.firstChild;
    assistantBubbles.set(messageId, bubble);
    messagesEl.appendChild(wrap);
  }
  bubble.textContent += text;
  scrollToBottom();
}

function appendErrorLine(text) {
  const div = document.createElement("div");
  div.className = "text-xs text-red-700 bg-red-50 border border-red-200 rounded px-2 py-1";
  div.textContent = text;
  messagesEl.appendChild(div);
  scrollToBottom();
}

function onTurnEnd(ev) {
  assistantBubbles.clear();
  sendBtn.disabled = false;
  inputEl.focus();
  if (ev.usage) {
    const { input_tokens = 0, output_tokens = 0 } = ev.usage;
    footerInfo.textContent = `last turn: ${input_tokens} in / ${output_tokens} out tokens`;
  }
}

function handleEvent(ev) {
  switch (ev.type) {
    case "ready":
      cwdEl.textContent = `cwd: ${ev.cwd}` +
        (ev.session_id ? ` · session ${ev.session_id.slice(0, 8)}` : " · new session");
      break;
    case "assistant_text":
      appendAssistantChunk(ev.message_id, ev.text);
      break;
    case "tool_use":
      appendToolUse(ev);   // Task 10
      break;
    case "tool_result":
      attachToolResult(ev); // Task 10
      break;
    case "turn_end":
      onTurnEnd(ev);
      break;
    case "error":
      appendErrorLine(ev.message || "error");
      sendBtn.disabled = false;
      break;
  }
}

// Placeholders for Task 10.
function appendToolUse(ev) { console.log("tool_use", ev); }
function attachToolResult(ev) { console.log("tool_result", ev); }

connect();
