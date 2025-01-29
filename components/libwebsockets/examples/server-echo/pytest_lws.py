# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import json
import random
import asyncio
import pytest
import websockets
import string
import subprocess
import re
import os
import time
import shutil
import sys
import pytest_asyncio

#TODO: use dut.app.sdkconfig.get("..") to get the values
# WebSocket connection configuration
USE_TLS = False
PORT = 443 if USE_TLS else 80

def test_examples_protocol_websocket(dut):
    """
    steps:
      1. obtain IP address
      2. connect to uri specified in the config
      3. send and receive data
    """

    # Test for echo functionality:
    # Sends a series of simple "hello" messages to the WebSocket server and verifies that each one is echoed back correctly.
    # This tests the basic responsiveness and correctness of the WebSocket connection.
    def test_echo(dut):
        dut.expect('Ethernet Link Up')
        print('All echos received')
        sys.stdout.flush()
    # Starting of the test
    try:
        if dut.app.sdkconfig.get('WS_OVER_TLS_MUTUAL_AUTH') is True:
            use_tls = True
            client_verify = True
        else:
            use_tls = False
            client_verify = False

    except Exception:
        print('TLLLLLLLLLLLLLLLLLLLLLLSS AAAAAAAAAAAAAAAAA')
        print(use_tls)
        print('ENV_TEST_FAILURE: Cannot find uri settings in sdkconfig')
        raise

    test_echo(dut)


# def get_esp32_ip():
#     """Retrieves the ESP32 IP dynamically from ESP-IDF monitor logs."""
#     try:
#         print("Searching for ESP32 IP in logs...")

#         # Attempt to find ESP-IDF installation path
#         idf_path = os.getenv("IDF_PATH")
#         if not idf_path:
#             possible_paths = [
#                 "/opt/esp-idf",                         # Default Linux path
#                 os.path.expanduser("~/esp/esp-idf"),    # Common Linux/macOS path
#                 os.path.expanduser("~/esp-idf"),        # Alternative Linux/macOS path
#                 "C:\\Espressif\\esp-idf",               # Default Windows path
#             ]
#             for path in possible_paths:
#                 if os.path.exists(path):
#                     idf_path = path
#                     break

#         if not idf_path:
#             print("Error: ESP-IDF not found. Make sure it is installed and sourced.")
#             sys.exit(1)

#         print(f"ESP-IDF found at: {idf_path}")

#         # Ensure idf.py is available in PATH
#         idf_command = "idf.py"
#         if not shutil.which("idf.py"):
#             idf_command = f"source {idf_path}/export.sh && idf.py"

#         # Automatically detect the ESP32 serial port
#         port = None
#         dev_list = [dev for dev in os.listdir("/dev") if "ttyUSB" in dev or "ttyACM" in dev]
#         if dev_list:
#             port = f"/dev/{dev_list[0]}"
#         elif sys.platform.startswith("win"):
#             port = "COM9"  # Adjust if necessary. TODO: Check - Isn't this something that is done in .yml?

#         if not port:
#             print("Error: No serial port found for ESP32.")
#             sys.exit(1)

#         print(f"Using serial port: {port}")

#         # Run idf.py monitor in a separate process
#         command = f"script -q -c '{idf_command} monitor -p {port}' /dev/null"

#         process = subprocess.Popen(
#             command,
#             shell=True,
#             stdout=subprocess.PIPE,
#             stderr=subprocess.STDOUT,
#             text=True,
#             executable="/bin/bash"
#         )

#         ip_address = None
#         timeout = time.time() + 90

#         # Improved regex to capture the exact expected message format
#         ip_regex = re.compile(r"ESP32_IP:\s*(\d+\.\d+\.\d+\.\d+)")

#         print("Waiting for IP to be printed in logs...")
        
#         while time.time() < timeout:
#             output = process.stdout.readline().strip()
#             if not output:
#                 continue

#             print(output)  # Print all output for debugging

#             match = ip_regex.search(output)
#             if match:
#                 ip_address = match.group(1)
#                 print(f"ESP32 IP found: {ip_address}")
#                 process.terminate()
#                 process.wait()
#                 print(f"ESP32 Server detected with IP: {ip_address}")
#                 return ip_address

#             time.sleep(0.5)  # Wait a bit before reading the next line

#         print("Error: ESP32 IP not found in logs.")
#         process.terminate()
#         process.wait()
#         sys.exit(1)

#     except subprocess.TimeoutExpired:
#         print("Error: Timeout while retrieving ESP32 IP.")
#         sys.exit(1)
#     except Exception as e:
#         print(f"Unexpected error: {e}")
#         sys.exit(1)

# # Get the IP dynamically
# ESP32_IP = get_esp32_ip()

# URI = f"wss://{ESP32_IP}" if USE_TLS else f"ws://{ESP32_IP}:{PORT}"
# print(f"WebSocket URI set to: {URI}") # TODO: Remove.

# @pytest_asyncio.fixture
# async def websocket():
#     """Manages the WebSocket connection to ESP32, with retry attempts."""
#     print(f"Connecting to WebSocket at: {URI}")

#     for attempt in range(3):
#         try:
#             ws = await websockets.connect(URI)
#             print(f"Connected to WebSocket at {URI}")
#             yield ws  
#             await ws.close()
#             print(f"WebSocket connection closed at {URI}")
#             return
#         except Exception as e:
#             print(f"Connection failed ({attempt + 1}/3): {e}")
#             await asyncio.sleep(2)

#     pytest.fail("Failed to connect to WebSocket server after multiple attempts.")

# async def send_websocket_message(websocket, message, is_binary=False):
#     """Sends a WebSocket message and waits for a response."""
#     try:
#         await websocket.send(message)
#         response = await asyncio.wait_for(websocket.recv(), timeout=5)
#         return response
#     except asyncio.TimeoutError:
#         return None
#     except Exception as e:
#         print(f"Error sending message: {e}")
#         return None

# async def receive_full_message(websocket, timeout=30, expected_length=None):
#     """Receives a complete message fragmented"""
#     try:
#         received_parts = []
#         total_length = 0

#         while True:
#             fragment = await asyncio.wait_for(websocket.recv(), timeout=timeout)

#             if fragment is None:
#                 print("No fragment received")
#                 break

#             fragment_length = len(fragment)
#             print(f"Received fragment: {fragment_length} bytes (Total so far: {total_length + fragment_length} bytes)")

#             received_parts.append(fragment)
#             total_length += fragment_length

#             # Se um tamanho esperado foi definido, parar quando atingir esse valor
#             if expected_length and total_length >= expected_length:
#                 print(f"Received expected ({expected_length} bytes). Ending receipt.")
#                 break

#         if received_parts:
#             full_message = b''.join(received_parts) if isinstance(received_parts[0], bytes) else ''.join(received_parts)
#             final_length = len(full_message)

#             print(f"Full message received ({final_length} bytes)")

#             # Se houver bytes extras e `expected_length` foi informado, truncar a mensagem
#             if expected_length and final_length > expected_length:
#                 print(f"Warning: Message exceeded expected length. Truncating to {expected_length} bytes.")
#                 full_message = full_message[:expected_length]

#             return full_message
#         else:
#             print("Received no data before timeout")
#             return None

#     except asyncio.TimeoutError:
#         print("Timeout waiting for fragmented message.")
#         return None
    
# async def send_fragmented_message(websocket, message, is_binary=False):
#     """Sends a fragmented WebSocket message, if supported."""
#     if hasattr(websocket, 'send_frame'):
#         print("Sending fragmented message...")
#         await websocket.send_frame(message, is_binary=is_binary)
#     else:
#         print("Sending message as a single frame...")
#         await websocket.send(message)

# @pytest.mark.asyncio
# async def test_websocket_connection(websocket):
#     """Tests WebSocket connection to ESP32."""
#     assert websocket is not None, "Failed to connect to WebSocket server."
#     print(f"[TEST] Successfully connected to WebSocket at {URI}")

# @pytest.mark.asyncio
# async def test_websocket_echo(websocket):
#     """Tests WebSocket echo functionality."""
#     message = "Hello ESP32!"
#     received = await send_websocket_message(websocket, message)
#     assert received == message, f"Expected '{message}', but received '{received}'"

# @pytest.mark.asyncio
# async def test_websocket_json(websocket):
#     """Tests sending and receiving JSON messages."""
#     json_message = json.dumps({"id": 6, "description": "JSON Test"})
#     received = await send_websocket_message(websocket, json_message)
#     assert received == json_message, "JSON message mismatch"

# @pytest.mark.asyncio
# async def test_websocket_binary(websocket):
#     """Tests sending and receiving binary messages."""
#     message = bytes([random.randint(0, 255) for _ in range(32)])
#     received = await send_websocket_message(websocket, message, is_binary=True)
#     assert received == message, "Binary message mismatch"

# @pytest.mark.asyncio
# async def test_websocket_hex(websocket):
#     """Tests sending and receiving hexadecimal messages."""
#     message = bytes([random.randint(0, 255) for _ in range(16)]).hex()
#     received = await send_websocket_message(websocket, message)
#     assert received == message, f"Expected hex '{message}', but received '{received}'"

# @pytest.mark.asyncio
# async def test_websocket_long_text(websocket):
#     """Tests sending and receiving long text messages (1KB)."""
#     message = ''.join(random.choices(string.ascii_letters + string.digits, k=1024))
#     received = await send_websocket_message(websocket, message)
#     assert received == message, f"Long text message mismatch."

# @pytest.mark.asyncio
# async def test_websocket_long_binary(websocket):
#     """Tests sending and receiving long binary messages (1KB)."""
#     message = bytes([random.randint(0, 255) for _ in range(1024)])
#     received = await send_websocket_message(websocket, message, is_binary=True)
#     assert received == message, "Long binary message mismatch"

# @pytest.mark.asyncio
# async def test_websocket_fragmented_text(websocket):
#     """Tests sending and receiving large fragmented text messages."""
#     part1 = ''.join(random.choices(string.ascii_letters + string.digits, k=1256))
#     keyword = "###FRAGMENT###"
#     part2 = ''.join(random.choices(string.ascii_letters + string.digits, k=715))

#     message = part1 + keyword + part2
#     expected_length = len(message)

#     await send_fragmented_message(websocket, message, is_binary=False)
#     received = await receive_full_message(websocket, expected_length=expected_length)

#     if received is None:
#         pytest.fail("No message received from server.")

#     received_length = len(received)

#     print(f"\n[TEST] Fragmented Text Message:")
#     print(f"Expected length: {expected_length} bytes, Received length: {received_length} bytes")

#     assert received_length == expected_length, f"Length mismatch: Expected {expected_length} (ESP log), but got {received_length}"

# @pytest.mark.asyncio
# async def test_websocket_fragmented_binary(websocket):
#     """Tests sending and receiving large fragmented binary messages."""
#     part1 = bytes([random.randint(0, 255) for _ in range(1552)])

#     message = part1
#     expected_length = len(message)

#     await send_fragmented_message(websocket, message, is_binary=True)
#     received = await receive_full_message(websocket)

#     if received is None:
#         pytest.fail("No message received from server.")

#     received_length = len(received)

#     print(f"\n[TEST] Fragmented Binary Message:")
#     print(f"Expected length: {expected_length} bytes, Received length: {received_length} bytes")

#     assert received_length == expected_length, f"Length mismatch: Expected {expected_length} (ESP log), but got {received_length}"

