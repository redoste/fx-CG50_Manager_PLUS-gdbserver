import socket
import sys

def encode_message(message):
    checksum = 0
    for byte in message:
        checksum += byte
    checksum %= 256
    return b"$" + message + b"#" + ("%02X" % checksum).encode()

def dump_region(sock, address):
    sock.send(encode_message(b"\xFFdump_region" + ("%08X" % address).encode()))
    ack = sock.recv(1, socket.MSG_WAITALL)
    if ack != b'+':
        raise Exception("Did not recive ACK byte from fxCG50gdb")
    return sock.recv(0x1000, socket.MSG_WAITALL)

def main():
    if len(sys.argv) != 5:
        print("Usage : %s HOST PORT PHYSICAL_ADDRESS NUMBER_OF_REGIONS" % sys.argv[0], file=sys.stderr)
        sys.exit(1)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((sys.argv[1], int(sys.argv[2])))

    address = int(sys.argv[3], 16) & 0xfffff000
    for _ in range(int(sys.argv[4])):
        sys.stdout.buffer.write(dump_region(sock, address))
        address += 0x1000
    sock.close()

if __name__ == "__main__":
    main()
