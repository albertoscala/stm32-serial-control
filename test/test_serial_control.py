import hashlib
import hmac
import socket
import struct
import time

KEY = bytes([
    0xF2, 0xE9, 0x8B, 0x1D, 0xB9, 0x56, 0xE5, 0x20,
    0x7E, 0xB4, 0xB6, 0x86, 0xCF, 0x67, 0xF9, 0xC5,
    0xD3, 0x98, 0x82, 0x6B, 0xDA, 0x63, 0x1C, 0x09,
    0x8E, 0x0D, 0x9A, 0x07, 0x7C, 0xA6, 0xDD, 0xF8,
])

OK = (1, True)

counter = 1

sock = socket.create_connection(("127.0.0.1", 4444))
time.sleep(1)


def frame(digit, ctr=None, key=KEY):
    global counter
    if ctr is None:
        counter += 1
        ctr = counter
    body = bytes([1, digit]) + bytes(7) + struct.pack("<I", ctr)
    return b"\xAA" + body + hmac.new(key, body, hashlib.sha256).digest()[:16] + b"\xFF"


def send(data):
    sock.sendall(data + bytes(100))


def read():
    reply = b""
    sock.settimeout(1.5)
    try:
        while True:
            reply += sock.recv(256)
    except socket.timeout:
        pass
    return [(reply[i + 1], reply[i + 2] == 1) for i in range(0, len(reply), 11)]


# Test:     valid commands are accepted
# Input:    10 correctly signed frames, digits 0 to 9
# Expected: 10 responses, all accepted
def test_valid_digits():
    data = b"".join(frame(d) for d in range(10))
    expected = [OK] * 10

    send(data)
    got = read()
    assert got == expected, got


# Test:     a message changed after signing is rejected
# Input:    valid frame for digit 5, with the digit changed to 7
# Expected: empty, no response
def test_altered_digit():
    f = frame(5)
    data = f[:2] + b"\x07" + f[3:]
    expected = []

    send(data)
    got = read()
    assert got == expected, got


# Test:     a changed counter is rejected
# Input:    valid frame with one bit of the counter changed
# Expected: empty, no response
def test_altered_counter():
    f = frame(5)
    data = f[:10] + bytes([f[10] ^ 1]) + f[11:]
    expected = []

    send(data)
    got = read()
    assert got == expected, got


# Test:     a message signed with the wrong key is rejected
# Input:    frame for digit 5 signed with an all-zero key
# Expected: empty, no response
def test_altered_wrong_key():
    data = frame(5, key=bytes(32))
    expected = []

    send(data)
    got = read()
    assert got == expected, got


# Test:     sending the same message twice is rejected
# Input:    a valid frame, then the exact same frame again
# Expected: 1 response, accepted (the second frame gets no response)
def test_replayed_same_frame():
    f = frame(3)
    data = f + f
    expected = [OK]

    send(data)
    got = read()
    assert got == expected, got


# Test:     reusing a counter with different content is rejected
# Input:    a valid frame, then a new frame with the same counter and another digit
# Expected: 1 response, accepted (the second frame gets no response)
def test_replayed_same_counter():
    f = frame(3)
    data = f + frame(4, ctr=counter)
    expected = [OK]

    send(data)
    got = read()
    assert got == expected, got


# Test:     an older counter is rejected
# Input:    a valid frame, then a frame with a lower counter
# Expected: 1 response, accepted (the second frame gets no response)
def test_replayed_old_counter():
    f = frame(3)
    data = f + frame(4, ctr=counter - 1)
    expected = [OK]

    send(data)
    got = read()
    assert got == expected, got


# Test:     an incomplete message is ignored
# Input:    only the first 20 of the 31 bytes of a frame
# Expected: empty, no response
def test_malformed_truncated():
    data = frame(8)[:20]
    expected = []

    send(data)
    got = read()
    assert got == expected, got


# Test:     a message with the wrong end byte is ignored
# Input:    a full frame with the last byte 00 instead of FF
# Expected: empty, no response
def test_malformed_wrong_end_byte():
    data = frame(8)[:-1] + b"\x00"
    expected = []

    send(data)
    got = read()
    assert got == expected, got


# Test:     random bytes are ignored
# Input:    80 bytes of garbage with no start byte
# Expected: empty, no response
def test_malformed_garbage():
    data = bytes(range(0x10, 0x60))
    expected = []

    send(data)
    got = read()
    assert got == expected, got


tests = [
    test_valid_digits,
    test_altered_digit,
    test_altered_counter,
    test_altered_wrong_key,
    test_replayed_same_frame,
    test_replayed_same_counter,
    test_replayed_old_counter,
    test_malformed_truncated,
    test_malformed_wrong_end_byte,
    test_malformed_garbage,
]

if __name__ == '__main__':
    for test in tests:
        try:
            test()
            print("PASS", test.__name__)
        except AssertionError as e:
            print("FAIL", test.__name__, e)