import socket
import sys
import json
import struct

def check_mesen_socket(socket_path):
    print(f"Connecting to {socket_path}...")
    try:
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.connect(socket_path)
        
        # Simple handshake or version check
        # Assuming JSON-line protocol or length-prefixed?
        # Based on docs, it's length-prefixed JSON.
        
        # Let's try to infer if we can just get a response.
        # Actually, let's just assert we can connect.
        print("Connected successfully.")
        client.close()
        return True
    except Exception as e:
        print(f"Failed to connect: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: mesen2_check.py <socket_path>")
        sys.exit(1)
        
    if check_mesen_socket(sys.argv[1]):
        sys.exit(0)
    else:
        sys.exit(1)
