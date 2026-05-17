"""UDP client for communicating with zedpet ESP32 firmware."""
import socket
import json

PORT = 19820
TIMEOUT = 2.0  # seconds to wait for ACK


def send_command(ip: str, cmd: str) -> str | None:
    """Send a command to the ESP32 and wait for ACK.

    Args:
        ip: ESP32 IP address
        cmd: one of idle/happy/sleep/talk/stretch/look

    Returns:
        The ACK action string, or None if timeout.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(TIMEOUT)

    payload = json.dumps({"cmd": cmd}).encode()
    sock.sendto(payload, (ip, PORT))

    try:
        data, addr = sock.recvfrom(256)
        response = json.loads(data.decode())
        return response.get("ack")
    except socket.timeout:
        return None
    finally:
        sock.close()
