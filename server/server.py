#!/usr/bin/env python3
import hashlib
import struct
import socket
from sys import argv

class HashServer:
    """
    A class that enables painless setup of a hashing server.
    Server is only able to handle one client at a time.
    Listens on port 2345 by default.

    Attributes
    ----------
    BUFF_SIZE : int
        constant used to restrict size of transfer iterations to 4kb (avoid mem waste)
    ALGO_SIZE : int
        max str length of hashing result
    UINT_32 : int
        alias for 4 bytes (UINT_32 size)
    supported : set
        supported hashing algos
    server : file descriptor
        socket to handle client connections

    Methods
    -------
    listen_loop
        Main loop for handling client connections

    recv_msg
        Receives messages from client
        Primarily used for receiving hashing algo

    get_algo_fc
        Receives algo type and number of files to process

    hash_file
        Hashes next file to be processed based on requested algo

    recv_file
        Receives next file from client
    """

    def __init__(self, port=2345): 

        print("Hash Server initializing on port", port) 
        self.BUFF_SIZE = 4096
        self.ALGO_SIZE = 128
        self.UINT_32 = 4
        self.supported = {'sha1', 'sha256', 'sha512', 'md5'}

        self.server = socket.socket()
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1) #allow port re-use
        self.server.bind(('', port)) 

    def listen_loop(self):
        self.server.listen(5)
        while True:
            self.client, self.addr = self.server.accept()
            fc = self.get_algo_fc()
            curr_file = 1
            while curr_file <= fc:
                hasher.recv_file()
                curr_file+=1
            self.client.close()
        self.server.close()

    def recv_msg(self):
        size = self.client.recv(self.UINT_32)
        if len(size) < 4:
            print("Invalid data")
            return
        size = struct.unpack('<I', size)[0] #turn into little endian int
        message = b''
        while size > 0:
            data = self.client.recv(self.BUFF_SIZE if self.BUFF_SIZE < size else size)
            message += data
            size -= len(data)
        return message

    def get_algo_fc(self):
        algo = self.recv_msg()
        if not algo:
            print("bad algo")
            self.client.close()
            return 0

        self.algo = algo.decode().rstrip()
        if self.algo not in self.supported:
            print("Invalid algo", self.algo)
            msg = "Invalid hashing algorithm\n".encode()
            self.client.send(msg)
            self.client.close()
            return 0
        else:
            print("Using " + self.algo + " hashing algorithm.")

        try:
            fc = self.client.recv(self.UINT_32)
        except OSError:
            print("Received bad file count")
            return 0
        fc = struct.unpack('<I', fc)[0] 
        return fc

    def hash_file(self, file):
        file_hash = None
        if self.algo == 'sha1':
            file_hash = hashlib.sha1(file).hexdigest()
        elif self.algo == 'sha256':
            file_hash = hashlib.sha256(file).hexdigest()
        elif self.algo == 'sha512':
            file_hash = hashlib.sha512(file).hexdigest()
        elif self.algo == 'md5':
            file_hash = hashlib.md5(file).hexdigest()
        return file_hash 

    def recv_file(self):
        data = b''
        size = self.client.recv(self.UINT_32)
        if len(size) < 4:
            print("Invalid data")
            return 
        size = struct.unpack('<I', size)[0]
        received = 0
        while len(data) < size:
            data += self.client.recv(self.BUFF_SIZE)
        data_hash = self.hash_file(data)
        self.client.send(data_hash.encode())


def print_usage():
    print("Usage: ./server.py [-p port]\n\
        Default port = 2345")
        

def get_port():
    port = None
    if len(argv) > 1:
        if argv[1] == '-h':
            print_usage()
        elif argv[1] == '-p': 
            try:
                port = int(argv[2])
            except ValueError:
                print("Must provide integer value for port")
                exit()
        else:
            print("Invalid flag provided")
            print_usage()
            exit()
    return port
            
        
if __name__ == "__main__":

    port = get_port()

    hasher = HashServer(port) if port else HashServer()
    hasher.listen_loop()