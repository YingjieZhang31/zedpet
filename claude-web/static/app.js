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

const toolNodes = new Map();  // tool_use_id -> { line, card, resultLineEl, resultDetailEl }

function summarizeInput(name, input) {
  if (!input || typeof input !== "object") return "";
  if (name === "Bash") return input.command || "";
  if (name === "Read" || name === "Edit" || name === "Write") return input.file_path || input.path || "";
  if (name === "Grep") return input.pattern || "";
  if (name === "Glob") return input.pattern || "";
  // Generic fallback: first 80 chars of JSON
  const s = JSON.stringify(input);
  return s.length > 80 ? s.slice(0, 80) + "…" : s;
}

function summarizeResult(content, isError) {
  if (!content) return isError ? "error" : "ok";
  const oneLine = content.replace(/\s+/g, " ").trim();
  return oneLine.length > 60 ? oneLine.slice(0, 60) + "…" : oneLine;
}

function appendToolUse(ev) {
  const wrap = document.createElement("div");
  wrap.className = "tool-event";

  // Compact one-liner
  const line = document.createElement("div");
  line.className = "tool-line items-center text-xs text-gray-500 gap-2";
  const resultLineEl = document.createElement("span");
  resultLineEl.className = "text-gray-400";
  resultLineEl.textContent = "…";
  line.innerHTML = `<span>🔧</span><span class="font-medium text-gray-700"></span><span class="truncate"></span><span>·</span>`;
  line.children[1].textContent = ev.name;
  line.children[2].textContent = summarizeInput(ev.name, ev.input);
  line.appendChild(resultLineEl);

  // Expanded card
  const card = document.createElement("details");
  card.className = "tool-card border rounded bg-gray-50";
  const summary = document.createElement("summary");
  summary.className = "cursor-pointer px-2 py-1 text-xs text-gray-700 flex items-center gap-2";
  summary.innerHTML = `<span>🔧</span><span class="font-medium"></span><span class="text-gray-500 truncate"></span>`;
  summary.children[1].textContent = ev.name;
  summary.children[2].textContent = summarizeInput(ev.name, ev.input);
  card.appendChild(summary);

  const body = document.createElement("div");
  body.className = "px-2 pb-2 text-xs space-y-2";
  const inputPre = document.createElement("pre");
  inputPre.className = "bg-white border rounded p-2 overflow-x-auto";
  inputPre.textContent = JSON.stringify(ev.input, null, 2);
  body.appendChild(labelled("input", inputPre));

  const resultDetailEl = document.createElement("pre");
  resultDetailEl.className = "bg-white border rounded p-2 overflow-x-auto text-gray-400";
  resultDetailEl.textContent = "(waiting…)";
  body.appendChild(labelled("output", resultDetailEl));
  card.appendChild(body);

  wrap.appendChild(line);
  wrap.appendChild(card);
  messagesEl.appendChild(wrap);
  scrollToBottom();

  toolNodes.set(ev.tool_use_id, { line, card, resultLineEl, resultDetailEl });
}

function attachToolResult(ev) {
  const node = toolNodes.get(ev.tool_use_id);
  if (!node) return;
  node.resultLineEl.textContent = summarizeResult(ev.content, ev.is_error);
  node.resultLineEl.className = ev.is_error ? "text-red-600" : "text-gray-500";
  node.resultDetailEl.textContent = ev.content || "";
  node.resultDetailEl.className = "bg-white border rounded p-2 overflow-x-auto " +
    (ev.is_error ? "text-red-700" : "text-gray-800");
}

function labelled(label, child) {
  const wrap = document.createElement("div");
  const lbl = document.createElement("div");
  lbl.className = "text-gray-500 mb-1";
  lbl.textContent = label;
  wrap.appendChild(lbl);
  wrap.appendChild(child);
  return wrap;
}

connect();
