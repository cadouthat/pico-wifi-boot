from crc import Calculator, Crc32
from enum import IntEnum
import selectors
import socket
import types
import os
import errno
import sys


OTA_PORT = 2222


# to be sent back by ota server
class OtaResponseCode(IntEnum):
    SUCCESS = 0
    STORAGE_FULL = 1
    CHECKSUM_FAILED = 2
    REBOOTING = 3


# socket lifecycle for the write handler
class WriteStatusCode(IntEnum):
    INIT = 0
    AWAIT_RESPONSE = 1
    PAYLOAD_READY = 2
    PAYLOAD_SENT = 3


# to signify result to main program
class FlashResultCode(IntEnum):
    SUCCESS = 0
    FAILURE = 1
    LOADING = 2


def make_checksum(buf):
    calc = Calculator(Crc32.CRC32)
    return calc.checksum(buf)


def read_bin(path):
    with open(path, mode='rb') as file:
        contents = file.read()
    return contents


# ask ota server if bytes are availabe with checksum for later verification
def pack_request(payload_size, checksum):
    buf = bytearray(12)
    buf[0:4] = b'OTA\n'
    buf[4:8] = payload_size.to_bytes(
        length=4, byteorder="little", signed=False)
    buf[8:12] = checksum.to_bytes(
        length=4, byteorder="little", signed=False)
    return buf


def get_response_status(buf):
    if buf[0:4].decode("utf-8") != "OTA\n":
        return -1
    return int.from_bytes(buf[4:5], byteorder="little",  signed=False)


def delete_socket(select, sock):
    select.unregister(sock)
    sock.shutdown(socket.SHUT_RDWR)
    sock.close()


# create socket and connect it to ota server
# also provide some instance-specific data for the read and write callbacks
# finally register the created socket with the event queue
def add_socket(ip, payload, checksum, select):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setblocking(False)
    err = sock.connect_ex((ip, OTA_PORT))
    if err != 0 and err != errno.EINPROGRESS:  # EINPROGRESS is expected as connect is not instantaneous
        print(f"error connecting socket to {ip}: {os.strerror(err)}")
        return
    events = selectors.EVENT_READ | selectors.EVENT_WRITE
    data = types.SimpleNamespace(
        addr=ip,
        payload=payload,
        bytes_sent=0,
        checksum=checksum,
        status=WriteStatusCode.INIT
    )
    select.register(sock, events, data=data)


# TODO: properly handle different results instead of just printing
def handle_read_event(select, sock, data):
    response = sock.recv(8)  # response should never be more than 5 bytes
    if (len(response) == 0):  # not sure if this can happen through select
        print(f'unexpected disconnect from client: {data.addr}')
        delete_socket(select, sock)
        return FlashResultCode.FAILURE
    else:
        response = get_response_status(response)

    # no errors yet - continue
    if response == OtaResponseCode.SUCCESS:
        if data.status == WriteStatusCode.AWAIT_RESPONSE:
            data.status = WriteStatusCode.PAYLOAD_READY
            data.bytes_sent = 0
        elif data.status == WriteStatusCode.PAYLOAD_SENT:
            print(f"payload sent successfully! closing connection with {
                  data.addr}")
            delete_socket(select, sock)
            return FlashResultCode.SUCCESS

    # ota server is rebooting into wifi bootloader - we must reconnect
    elif response == OtaResponseCode.REBOOTING:
        delete_socket(select, sock)
        add_socket(data.addr, data.payload, data.checksum, select)

    # fatal error
    elif response == OtaResponseCode.STORAGE_FULL:
        print(f"ota server @ {data.addr}: storage full")
        delete_socket(select, sock)
    elif response == OtaResponseCode.CHECKSUM_FAILED:
        print(f"ota server @ {data.addr}: checksum failed")
        delete_socket(select, sock)

    return FlashResultCode.FAILURE


# there are two types of write events
# we either request to send the data or we actually send the data
def handle_write_event(select, sock, data):
    if data.status == WriteStatusCode.INIT:
        sent = sock.send(pack_request(
            len(data.payload[data.bytes_sent:]), data.checksum))
        data.bytes_sent += sent
        # sent error check?
        data.status = WriteStatusCode.AWAIT_RESPONSE

    elif data.status == WriteStatusCode.PAYLOAD_READY:
        sent = sock.send(data.payload[data.bytes_sent:])
        data.bytes_sent += sent
        if data.bytes_sent == len(data.payload):
            data.status = WriteStatusCode.PAYLOAD_SENT


def event_loop(select, result_map):
    while len(select.get_map()):  # exit when all sockets are closed
        events = select.select(20)
        if not events:
            print("event queue reached timeout - check your connection")
            break
        for key, mask in events:
            sock = key.fileobj
            data = key.data
            if mask & selectors.EVENT_READ:
                result_map[data.addr] = handle_read_event(select, sock, data)
            if mask & selectors.EVENT_WRITE:
                handle_write_event(select, sock, data)
    print("exiting event loop")


def flash_to_all(firmware_path, ip_addresses):
    payload = read_bin(firmware_path)
    checksum = make_checksum(payload)
    select = selectors.DefaultSelector()
    result_map = {}
    for ip in ip_addresses:
        add_socket(ip, payload, checksum, select)
        result_map[ip] = FlashResultCode.FAILURE
    event_loop(select, result_map)
    return result_map


def main():
    if len(sys.argv) < 3:
        print("usage: python flash.py addr1 [.. addrN] binary")
        return
    addreses = sys.argv[1:-1]
    path = sys.argv[-1:][0]
    results = flash_to_all(path, addreses)
    print(results)


if __name__ == "__main__":
    main()
