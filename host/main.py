#!/usr/bin/env python3
"""zedpet Control — GUI host for M5Cardputer pet."""
import tkinter as tk
from tkinter import messagebox, simpledialog
import threading
import queue
import udp_client


class App:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("ZEDPET Control")
        self.root.resizable(False, False)

        self.esp_ip: str | None = None
        self.ack_queue = queue.Queue()

        self._build_ui()
        self._ask_ip()

    def _build_ui(self):
        # Title
        title = tk.Label(self.root, text="ZEDPET Control",
                         font=("Helvetica", 16, "bold"), pady=10)
        title.pack()

        # Action buttons
        frame = tk.Frame(self.root)
        frame.pack(padx=20, pady=10)

        labels = ["IDLE", "HAPPY", "SLEEP", "TALK", "STRETCH", "LOOK"]
        cmds = ["idle", "happy", "sleep", "talk", "stretch", "look"]

        for label, cmd in zip(labels, cmds):
            btn = tk.Button(frame, text=label, width=20, height=2,
                            font=("Helvetica", 12),
                            command=lambda c=cmd: self._send(c))
            btn.pack(pady=4)

        # Status bar
        self.status = tk.Label(self.root, text="Ready",
                               font=("Helvetica", 10), fg="gray",
                               pady=10)
        self.status.pack()

    def _ask_ip(self):
        ip = simpledialog.askstring(
            "ESP32 IP", "Enter ESP32 IP address:",
            parent=self.root)
        if ip:
            self.esp_ip = ip.strip()
            self.status.config(text=f"Target: {self.esp_ip}")
        else:
            self.status.config(text="No IP set — edit and restart")

    def _send(self, cmd: str):
        if not self.esp_ip:
            self.status.config(text="No IP set!", fg="red")
            return

        self.status.config(text=f"Sending {cmd}...", fg="gray")

        def task():
            ack = udp_client.send_command(self.esp_ip, cmd)
            self.ack_queue.put((cmd, ack))

        t = threading.Thread(target=task, daemon=True)
        t.start()
        self.root.after(100, self._check_ack)

    def _check_ack(self):
        try:
            cmd, ack = self.ack_queue.get_nowait()
            if ack:
                self.status.config(text=f"ACK {ack} OK", fg="green")
            else:
                self.status.config(text="No response", fg="red")
        except queue.Empty:
            self.root.after(100, self._check_ack)


def main():
    root = tk.Tk()
    app = App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
